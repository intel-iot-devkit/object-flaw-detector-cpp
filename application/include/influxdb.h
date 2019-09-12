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

/**
 * @brief Header file to create database and write the data to InfluxDB
 */

# pragma once
# include <curl/curl.h>
# include <iostream>
# include <vector>
# include <regex>

/**
 * @brief namespace for InfluxDB class and data structure
 */
namespace influx
{

    /**
     * @brief Structure data for managing and querying the data to be written in InfluxDB
     */
    static struct Data
    {
        private:
            bool is_measure = false;
            bool is_time = false;
            bool is_field = false;
            long long int time = 0;
            std::string measure;
            std::vector<std::string> field_data;
            std::string tag_data;

        public:

            /**
             * @brief Takes the measurement name from the user
             * @param value - Name of the measurement
             */
            void add_measure(const std::string& value);

            /**
             * @brief Takes the tag key and value and add to the query string
             * @param key - Tag Key
             * @param value - Tag value
             */
            
            void add_tag(const std::string& key, const std::string& value);

            void add_tag(const std::string& key, short value);
            
            void add_tag(const std::string& key, int value);
            
            void add_tag(const std::string& key, double value);
            
            void add_tag(const std::string& key, long value);
            
            void add_tag(const std::string& key, long long value);

        
            /**
             * @brief Takes the field key and value and add to the query string
             * @param key - field Key
             * @param value - field value
             */
                
            void add_field(const std::string& key, const std::string& value);
            
            void add_field(const std::string& key, short value);
            
            void add_field(const std::string& key, int value);
            
            void add_field(const std::string& key, double value);
            
            void add_field(const std::string& key, long value);
            
            void add_field(const std::string& key, long long value);
            
            void add_timestamp(long long _time);

            /**
             * @brief Build the data string (data to be written to influxDB) that will be sent as data using InfluxDB Rest API
             * @return data string
             */

            std::string build_query();

            /**
             * @brief Add an escape character '\' in case on " ", "," and "=" in string values of key
             * Reference - InfluxDB write protocol
             * @param data - data to be formatted
             * @param measure - True if it is a measurement value
             * @return - formatted string
             */

            std::string add_escape_seq(std::string data, bool measure);


    } data;

    /**
     * @brief InfluxDB class to manage and write the data to the database
     */
    class InfluxDB
    {
        private:
            std::string host;
            int port;
            std::string url;
            FILE* file = fopen("../influx-logs.txt", "w");

        public:

            /**
             * @brief Default constructor. If host and port are not given, initial it by default to localhost:8086
             */
            InfluxDB();

            /**
             * @brief Paramaterised constructor
             * @param host - InfluxDB hostname
             * @param port - InfluxDB port
             */

            InfluxDB(std::string host, int port);

            /**
             * @brief Post the query using InfluxDB Rest API
             * @param _url - url on which request has to be posted
             * @param data - data to be sent
             * @return - Request status
             */
            
            CURLcode http_post(std::string _url, std::string data);

            /**
             * @brief Creates the database in InfluxDB
             * @param db_name - Name of the database to be created
             * @return -1 in case of error else 0
             */
            
            int create_database(const std::string& db_name);  

            /**
             * @brief Preprocess the data to be written to the database
             * @param db_name - Name of the database in which data has to written
             * @param data - Data to be written to the database
             * @return -1 in case of error else 0
             */            
            int write_point(std::string db_name, influx::Data data); 
    };
};
