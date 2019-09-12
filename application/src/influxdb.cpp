/*
 * Copyright (c) 2018-2019 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

# include "influxdb.h"

influx::InfluxDB::InfluxDB()
{
    host = "localhost";
    port = 8086;
    url = "http://localhost:8086";
}

influx::InfluxDB::InfluxDB(std::string host, int port)
{
    this->host = host;
    this->port = port;
    url = "http://" + host + ":" + std::to_string(port);
}


CURLcode influx::InfluxDB::http_post(std::string _url, std::string data)
{
    CURL *handle;
    CURLcode status;
    handle = curl_easy_init();
    curl_easy_setopt(handle, CURLOPT_URL, _url.c_str());
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)file);
    if (handle)
    {
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, data.c_str());

        // Send the HTTP POST request
        status = curl_easy_perform(handle);
        curl_easy_cleanup(handle);
        handle = NULL;
    }
    return status;
}


int influx::InfluxDB::create_database(const std::string& db_name)
{
    CURLcode status;
    std::string _url = url + "/query";
    std::string _query = "q=CREATE DATABASE \"" + db_name + "\"";
    status = http_post(_url, _query);
    if (status != CURLE_OK)
    {
        printf("Curl failed with code %d (%s)n", status, curl_easy_strerror(status));
        return -1;
    }
    return 0;
}

int influx::InfluxDB::write_point(std::string db_name, influx::Data data)
{
    CURLcode status;
    std::string _url = url + "/write?db=" + db_name;
    std::string _data = data.build_query();

    if (_data == "")
    {
        std::cout<<"ERROR:: Error occured while writing the data. Please check the Format of the data\n";
        return -1;
    }
    status = http_post(_url, _data.c_str());
    if (status != CURLE_OK)
    {
        printf("Curl failed with code %d (%s)n", status, curl_easy_strerror(status));
        return -1;
    }
    return 0;
}


void influx::Data::add_measure(const std::string& value)
{
    if (is_measure)
    {
        std::cout<<"WARNING:: Measurement has been added more than once. Taking the first value!"<<std::endl;
        return;
    }
    measure = add_escape_seq(value, true);
    is_measure = true;
}

void influx::Data::add_tag(const std::string& key, const std::string& value)
{
    tag_data = tag_data + "," + add_escape_seq(key, false) + "=" + add_escape_seq(value, false);    
}

void influx::Data::add_tag(const std::string& key, short value)
{
    tag_data = tag_data + "," + add_escape_seq(key, false) + "=" + std::to_string(value);    
}

void influx::Data::add_tag(const std::string& key, int value)
{
    tag_data = tag_data + "," + add_escape_seq(key, false) + "=" + std::to_string(value);    
} 

void influx::Data::add_tag(const std::string& key, double value)
{
    tag_data = tag_data + "," + add_escape_seq(key, false) + "=" + std::to_string(value);    
}

void influx::Data::add_tag(const std::string& key, long value)
{
    tag_data = tag_data + "," + add_escape_seq(key, false) + "=" + std::to_string(value);    
}

void influx::Data::add_tag(const std::string& key, long long value)
{
    tag_data = tag_data + "," + add_escape_seq(key, false) + "=" + std::to_string(value);    
}

void influx::Data::add_field(const std::string& key, const std::string& value)
{
    field_data.push_back(add_escape_seq(key, false) + "=\"" + value + "\""); 
    is_field = true;   
}

void influx::Data::add_field(const std::string& key, short value)
{
    field_data.push_back(add_escape_seq(key, false) + "=" + std::to_string(value));     
    is_field = true;
}

void influx::Data::add_field(const std::string& key, int value)
{
    field_data.push_back(add_escape_seq(key, false) + "=" + std::to_string(value));         
    is_field = true;
} 

void influx::Data::add_field(const std::string& key, double value)
{
    field_data.push_back(add_escape_seq(key, false) + "=" + std::to_string(value));     
    is_field = true;
}

void influx::Data::add_field(const std::string& key, long value)
{
    field_data.push_back(add_escape_seq(key, false) + "=" + std::to_string(value));
    is_field = true;
}

void influx::Data::add_field(const std::string& key, long long value)
{
    field_data.push_back(add_escape_seq(key, false) + "=" + std::to_string(value));     
    is_field = true;
}

void influx::Data::add_timestamp(long long _time)
{
    time = _time;
    is_time = true;
}

std::string influx::Data::build_query()
{
    std::string _query;

    if ( !is_measure )
    {
        std::cout<<"ERROR:: Measurement not added!"<<std::endl;
        return "";
    }
    if ( !is_field )
    {
        std::cout<<"ERROR:: Format error. No fields found!"<<std::endl;
        return "";
    }

    for (auto field : field_data)
    {
        if(_query.empty())
        {
            _query = field;
            continue;
        }
        _query = _query + "," + field;
    }

    if ( is_time )
    {
        return (measure + tag_data + " " + _query + " " + std::to_string(time));
    }
    else
    {
        return (measure + tag_data + " " + _query);
    }
}

std::string influx::Data::add_escape_seq(std::string data, bool measure)
{
        std::string _data = data;
        if (data.find_first_of(",") != std::string::npos)
        {
            _data = std::regex_replace(_data, std::regex(","), "\\,");
        }
        if (data.find_first_of(" ") != std::string::npos)
        {
            _data = std::regex_replace(_data, std::regex(" "), "\\ ");
        }
        if (data.find_first_of("=") != std::string::npos && !measure)
        {
            _data = std::regex_replace(_data, std::regex("="), "\\=");
        }

        return _data;
}


