#include<request.h>
#include<string>
#include<vector>
#include<sstream>
#include<algorithm>
#include<stdio.h>
#include<iostream>
#include<exceptions.h>
using namespace std;


std::vector<std::string> SUPPORTED_VERSIONS = {"HTTP/1.1"};
std::vector<std::string> SUPPORTED_METHODS = {"GET", "POST", "PUT", "DELETE"};

// Parses headers from the given lines and fills the headers map
void Request::parseHeaders(vector<string> headerLines){
    // Skip first line (request line)
    for(size_t i=1; i<headerLines.size(); i++){
        string line = headerLines[i];
        size_t delimPos = line.find(":");
        if(delimPos == string::npos){
            throw BadRequestException("Invalid header line: " + line);
        }

        if(delimPos == 0){
            throw BadRequestException("Empty header key in line: " + line);
        }

        string key = line.substr(0, delimPos);
        string value = line.substr(delimPos + 1);
        //trim whitespace from value
        size_t first = value.find_first_not_of(" \t");
        size_t last  = value.find_last_not_of(" \t");
        if (first != std::string::npos)
            value = value.substr(first, last - first + 1);
        else
            value.clear();

        transform(key.begin(), key.end(), key.begin(), ::tolower);
        headers[key] = value;
    }

    if (headers.find("host") == headers.end()) {
        throw BadRequestException("Missing Host header");
    }
}

// Parses the request line and fills method, path, and httpVersion
void Request::parseRequestLine(const string &requestLine){
    stringstream ss(requestLine);
    ss >> method >> uri >> httpVersion;
    if (ss.fail() || method.empty() || uri.empty() || httpVersion.empty())
    throw BadRequestException("Invalid request line");

    if(find(SUPPORTED_METHODS.begin(), SUPPORTED_METHODS.end(), method) == SUPPORTED_METHODS.end()){
        throw NotSupportedException("HTTP Method " + method + " is not supported");
    }

    if(find(SUPPORTED_VERSIONS.begin(), SUPPORTED_VERSIONS.end(), httpVersion) == SUPPORTED_VERSIONS.end()){
        throw NotSupportedException("HTTP Version " + httpVersion + " is not supported");
    }
}

// Parses query parameters from the path and fills the queryParams map
//To do: URL decode the parameters
void Request::parseQueryParams(){
    size_t queryPos = uri.find("?");
    path = uri;
    if(queryPos == string::npos){
        return; // No query params
    }
    string queryString = path.substr(queryPos + 1);
    path = path.substr(0, queryPos);

    stringstream ss(queryString);
    string param;
    while(getline(ss, param, '&')){
        size_t eqPos = param.find("=");
        if(eqPos != string::npos){
            string key = param.substr(0, eqPos);
            string value = param.substr(eqPos + 1);
            queryParams[key] = value;
        }else{
            queryParams[param] = "";
        }
    }
}


Request::Request(string headers, string requestBody){
    body = requestBody;
    stringstream ss(headers);
    vector<string> lines;
    string line;
    //parse lines, split the string by \n and further remove \r if present
    while(getline(ss, line, '\n')){
        if(line.back() == '\r'){
            line.pop_back();
        }
        if(!line.empty()){
            lines.push_back(line);
        }
    }

    if(lines.size() == 0){
        throw BadRequestException("Empty request");
    }

    parseRequestLine(lines[0]);
    parseHeaders(lines);
    parseQueryParams();

}

//Clear all the request details so that we can reuse the same request to store the new request during a keep-alive connection. 
void Request::clear(){
    method.clear();
    uri.clear();
    path.clear();
    httpVersion.clear();
    body.clear();
    headers.clear();
    queryParams.clear();
    params.clear();
}