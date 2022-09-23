package javatest;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Base64;
import java.util.Properties;
import org.apache.kafka.clients.producer.KafkaProducer;  
import org.apache.kafka.clients.producer.Producer;  
import org.apache.kafka.clients.producer.ProducerRecord;

import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import javatest.ExternalDataSet;
import javatest.ExternalDataSet.IOT;

public class JavaTest {
    public static char[] getChars(byte[] bytes) {
        Charset cs = Charset.forName("ISO646-US");
        ByteBuffer bb = ByteBuffer.allocate(bytes.length);
        bb.put(bytes).flip();
        CharBuffer cb = cs.decode(bb);
        return cb.array();
    }

    public static void main(String args[]) throws InvalidProtocolBufferException, UnsupportedEncodingException {

        Properties props = new Properties();  
        props.put("bootstrap.servers", "10.3.9.107:9092");
        props.put("key.serializer", "org.apache.kafka.common.serialization.StringSerializer");
        props.put("value.serializer", "org.apache.kafka.common.serialization.ByteArraySerializer");

        Producer<String, byte[]> producer = new KafkaProducer<String, byte[]>(props);

        ExternalDataSet.InteractionPoint.Builder ip = ExternalDataSet.InteractionPoint.newBuilder();
        ExternalDataSet.InteractionArea.Builder ia = ExternalDataSet.InteractionArea.newBuilder();
        ExternalDataSet.InstructionInfo.Builder builder = ExternalDataSet.InstructionInfo.newBuilder();
        ExternalDataSet.AlgorithmParm.Builder ap = ExternalDataSet.AlgorithmParm.newBuilder();
        ExternalDataSet.CameraInfo.Builder ci = ExternalDataSet.CameraInfo.newBuilder();

        ip.setHorizontal(10);
        ip.setOrdinate(20);
        ia.addAllPoints(0, ip);

        ip.setHorizontal(30);
        ip.setOrdinate(40);
        //ia.setAllPoints(0, ip);
        ia.addAllPoints(0, ip);

        ia.setHeight(1280);
        ia.setWidth(1920);
        ia.setShutDownArea(false);
        ia.setInterestType(1);

        ap.setRegion(ia);
        ap.setNms((float)0.6);
        ap.setThreshold((float) 0.7);

        ap.setCameraId("333d53fb18fa4692b40fa5c03a0d2eda");
        builder.addAparms(ap);

        ci.setCameraUrl("rtsp://admin:12345@192.168.26.3:554/h264/ch1/main/av_stream");
        ci.setDestUrl("rtmp://10.3.10.113/live/");
        ci.setPushFlow(true);
        ci.setCameraName("test");
        ci.setCameraId("333d53fb18fa4692b40fa5c03a0d2eda");
        builder.addCameraInfos(ci);

        builder.setIot(IOT.ADD);
        builder.setTimeStamp(1653980701);
        builder.setAlgorithmType(0);
        builder.setAlgorithmId("015f28b9df1bdd36427dd976fb73b29d");
        builder.setContainerID("4ba2fa9c7697");
        builder.setCurrentId("6256970e47746b607c5412db");
		
        ExternalDataSet.InstructionInfo instruct_info = builder.build();
        System.out.println("getSerializedSize:" + instruct_info.getSerializedSize());
        byte[] bs = instruct_info.toByteArray();

        //char[] c = getChars(bs);

        ExternalDataSet.InstructionInfo xxx = ExternalDataSet.InstructionInfo.parseFrom(bs);
        System.out.println("protobuf反序列化大小: " + xxx.getAparms(0).getRegion().getAllPointsCount());
        
        //String values = Arrays.toString(c);   
        //String values = String.valueOf(c);
        // System.out.println("byte length: " + bs.length);
        // System.out.println("values: " + values);
        // System.out.println("values size:" + values.length());
        producer.send(new ProducerRecord<String, byte[]>("gsafety_instruction", bs));
        producer.close();

        Peaple peaple = new Peaple("Tom");
        peaple.myName();
    }
}
