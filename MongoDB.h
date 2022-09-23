/*
Copyright (C) 2020 - 2021, Beijing Chen An Co.Ltd.All rights reserved.
File Name: HttpClient.h
Description: Mongodb
Change History:
Date         Version      Changes
2021/06/15   1.0.0.0      Create
*/
#ifndef _MONGODB_H_
#define _MONGODB_H_

// #include <curl.h>
#include <iostream>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <typeinfo>
#include <string>
using namespace std;

class MongoDB {
public:
	MongoDB();
	~MongoDB();

    /**
     * @brief 初始化mongo client
     * @param  "mongodb://cvap:Gsafety.@10.3.9.106:27017"
     * @return true  初始化成功
     */

    bool Init(std::string&);

    /**
     * @brief 按照参数查询，返回查询出来的结果
     * 
     * @param collections 为表名
     * @param msg 传进来的条件是一个map
     * @param res 返回的结果是一个vector<string>类型
     */
    template<typename TT>
    void  mongoQueryMatching(const std::string& collections, TT &msg,std::vector<string> &res)
    {
        bsoncxx::builder::stream::document filter_builder;
        auto collection=(*m_conn)["cvap"][collections.c_str()];

        for(auto it=msg.begin();it!=msg.end();it++)
            filter_builder<<it->first<<it->second;

        auto cursor = collection.find(filter_builder.view());

        for (auto&& doc : cursor)
        {
            std::string ret = std::string(bsoncxx::to_json(doc) );
            res.push_back(ret);
        }
    }

    /**
     * @brief 查询指定表中的所有数据
     * 
     * @param collections 表名
     * @param res 返回值是vector<string>
     */

    void  mongoSelectAll(const std::string& collections,std::vector<string>& res);

    /**
     * @brief 往表中插入数据
     * @param 第一个参数是表名  第二个参数是要插入的数据
     * @return true 插入成功
     * @return false 插入失败
     */

    bool mongoInsertMany(const std::string&, const std::string &);


    /**
     * @brief 根据条件删除表中的数据
     * 
     * @param collections 表的名称
     * @param msg 表的数据
     * @return true 删除成功
     * @return false 删除失败
     */
    
   
    void mongoDeleteMatching(const std::string& collections);

    /**
     * @brief 清空表数据
     * 
     * @param collections 表名
     */

    void mongoDeleteMany(const std::string &collections,std::map<std::string,std::string>&);

   /**
    * @brief 更新表数据
    * 
    * @param table 
    * @param table 表名字
    * @param str1  查找的key
    * @param str2  查找的value
    * @param str3  更新的key
    * @param str4  更新的value
    * @return true 更新成功
    * @return false 更新失败
    */


    bool mongoUpdate(const std::string& table, std::string &msg,std::string &res);



    /**
     * @brief 多条件更新
     * 
     * @param table 表名字
     * @param input 入参json
     * @param update 更新的值
     * @return true 
     * @return false 
     */

    bool mongoUpdateMany(const std::string &table,std::string  &input,std::string &update);




private:

    static mongocxx::instance inst;
    std::unique_ptr<mongocxx::client> m_conn;
};

#endif /* _MONGODB_H_ */

