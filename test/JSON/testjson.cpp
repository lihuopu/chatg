#include "json.hpp"
using json = nlohmann::json;

#include <vector>
#include <iostream>
#include <map>
#include <string>
using namespace std;


void fun3(){
    json js;
    map<int ,string> m;
    m.insert({1,"黄山"});
    m.insert({2,"话山"});
    m.insert({3,"啦山"});
    m.insert({4,"第山"});
    js["path"] = m;
    cout <<js<<endl;

}
int main(){
    fun3();
    return 0;
}
