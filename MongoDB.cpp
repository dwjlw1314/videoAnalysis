#include "MongoDB.h"
// c++ --std=c++11 MongoDB.cpp -o test $(pkg-config --cflags --libs libmongocxx)

// 使用了template成员函数,该语句导致abort
// mongocxx::instance MongoDB::inst{};

MongoDB::MongoDB()
{

}

MongoDB::~MongoDB()
{


}

bool MongoDB::Init(std::string& uri)
{
 
    m_conn = std::unique_ptr<mongocxx::client>(new mongocxx::client({mongocxx::uri{uri}}));
   
    return true;
}

void MongoDB::mongoSelectAll(const std::string& collections,std::vector<string> &res)
{

    auto collection=(*m_conn)["cvap"][collections.c_str()];
    auto cursor = collection.find({});

    for (auto && doc : cursor)
    {
        std::string ret = std::string(bsoncxx::to_json(doc) );
        res.push_back(ret);
    }

}

bool MongoDB::mongoInsertMany(const std::string& table, const std::string &data)
{

    //创建数据记录行/文档
    bsoncxx::builder::stream::document document{};

    //数据库/表, 要求性能则需要转成成员变量
    auto collection = (*m_conn)["cvap"][table.c_str()];
    
    //插入数据
    try {
        collection.insert_one(bsoncxx::from_json(data));
        return true;
    }
    catch(...)
    {
        std::cout << "mongo_insert Exception" << std::endl;
        
    }
    return false;
}

void MongoDB::mongoDeleteMany(const std::string& collections,std::map<std::string,std::string>& mymap)
{
    auto collection=(*m_conn)["cvap"][collections.c_str()];

    bsoncxx::builder::stream::document filter_builder;

    for(std::map<string,string>::iterator it=mymap.begin();it!=mymap.end();it++)
    {
        std::string key=it->first;
        std::string value=it->second;
        
        filter_builder<<key<<value;
    }

    collection.delete_many(filter_builder.view());
}


void MongoDB::mongoDeleteMatching(const std::string &collections)
{
    auto collection=(*m_conn)["cvap"][collections.c_str()];
    collection.delete_one((bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("name","test"))));

}

bool MongoDB::mongoUpdate(const std::string& table, std::string &msg,std::string &res)
{
    //数据库/表, 要求性能则需要转成成员变量
    auto collection = (*m_conn)["cvap"][table.c_str()];

    bsoncxx::stdx::optional<mongocxx::result::update> result =
    collection.update_many(bsoncxx::from_json(msg).view(),bsoncxx::from_json(res).view());

    std::cout<<"update count:" << result->modified_count() << std::endl;
    if (result->modified_count())
    {
        return true;
    }
    else
    {
        return false;
    }

}



bool MongoDB::mongoUpdateMany(const std::string &table,std::string  &input,std::string &update)
{

   //数据库/表, 要求性能则需要转成成员变量
    auto collection = (*m_conn)["cvap"][table.c_str()];

    bsoncxx::stdx::optional<mongocxx::result::update> result =
    collection.update_many(bsoncxx::from_json(input).view(),bsoncxx::from_json(update).view());

    std::cout<<"update count:" << result->modified_count() << std::endl;
    if (result->modified_count())
    {
        return true;
    }
    else
    {
        return false;
    }

}

