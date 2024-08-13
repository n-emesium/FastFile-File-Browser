#include <iostream>
#include <fstream>
#include "libraries/json.hpp"
#include <vector>

using namespace std;
using json = nlohmann::json;

int main()
{
    // json::ondemand::parser parser;
    // json::padded_string jsonFile = json::padded_string::load("fileIndex.json");
    // json::ondemand::document index = parser.iterate(jsonFile);
    //json::dom::object obj;
    //json::dom::element element;
    //obj["He"] = "1";
    json obj;
    obj['HM'].push_back('h');
    obj['HM'].push_back('n');


    cout << obj['HM'];

}