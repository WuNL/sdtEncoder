//
// Created by sdt on 18-8-8.
//

#ifndef ENCODER_JSONPARSER_H
#define ENCODER_JSONPARSER_H

#include <string>
#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>

#include "encoder.h"

using namespace boost::property_tree;

int jsonParser (std::string &rawJson, initParams &params)
{
    ptree pt;                       //define property_tree object
    std::stringstream ss(rawJson);
    try
    {
        read_json(ss, pt);          //parse json
    } catch (ptree_error &e)
    {
        return 1;
    }
    try
    {
        params.encoder_name = pt.get<std::string>(
                "encoder_name", "");// + "_" + pt.get<std::string>("v_width") + "_" + pt.get<std::string>("v_height");
        params.codec = pt.get<std::string>("codec", "h264");
        params.v_width = pt.get<int>("v_width", 640);
        params.v_height = pt.get<int>("v_height", 480);
        params.v_gop = pt.get<int>("v_gop", 300);
        params.packetMode = pt.get<int>("packetMode", 0);
        params.framerate = static_cast< int>(pt.get<float>("max_fps", 30));
        params.bitrate = pt.get<int>("target_kbps", 3000);
        if (params.bitrate < 1000)
            params.bitrate = 1000;
        params.dstIP = pt.get<std::string>("dstIP", "127.0.0.1");
        params.port = pt.get<int>("port", 12345);
        params.max_payload_len = pt.get<int>("payload_len", 1400);

        params.deviceID = pt.get<int>("deviceID", 12345);

        if (params.encoder_name == std::string(""))
            return 1;
        return 0;
    }
    catch (...)
    {
        printf("json parser error!\n");
        return 1;
    }


}

int jsonParserDestroy (std::string &rawJson, destroyParams &params)
{
    ptree pt;                       //define property_tree object
    std::stringstream ss(rawJson);
    try
    {
        read_json(ss, pt);          //parse json
    } catch (ptree_error &e)
    {
        return 1;
    }
    try
    {
        params.encoder_name = pt.get<std::string>(
                "encoder_name");// + "_" + pt.get<std::string>("v_width") + "_" + pt.get<std::string>("v_height");
        params.success = true;
    }
    catch (...)
    {
        printf("json parser error!\n");
        params.success = false;
        return 1;
    }
}

int jsonParserForceKeyFrame (std::string &rawJson, forceKeyFrameParams &params)
{
    ptree pt;                       //define property_tree object
    std::stringstream ss(rawJson);
    try
    {
        read_json(ss, pt);          //parse json
    } catch (ptree_error &e)
    {
        return 1;
    }
    try
    {
        params.encoder_name = pt.get<std::string>(
                "encoder_name");// + "_" + pt.get<std::string>("v_width") + "_" + pt.get<std::string>("v_height");
        params.success = true;
    }
    catch (...)
    {
        printf("json parser error!\n");
        params.success = false;
        return 1;
    }
}

int jsonParserUpdateBitrate (std::string &rawJson, initParams &params)
{
    ptree pt;                       //define property_tree object
    std::stringstream ss(rawJson);
    try
    {
        read_json(ss, pt);          //parse json
    } catch (ptree_error &e)
    {
        return 1;
    }
    try
    {
        params.encoder_name = pt.get<std::string>(
                "encoder_name", "");// + "_" + pt.get<std::string>("v_width") + "_" + pt.get<std::string>("v_height");
        params.bitrate = pt.get<int>("target_kbps", 3000);
        if (params.bitrate < 1000)
            params.bitrate = 1000;
        params.framerate = static_cast< int>(pt.get<float>("max_fps", 30));
        return 0;
    }
    catch (...)
    {
        printf("json parser error!\n");
        return 1;
    }


}

#endif //ENCODER_JSONPARSER_H
