#ifndef USER_H
#define USER_H

#include <string>
using namespace std;

//匹配User表的ORM类
class User{

    public:

    User(int id =1,string name = "",string pwd = "",string state = "offline"){
        this->id =id;
        this->name = name;
        this->password = password;
        this->state = state;
    }

    //设置成员变量方法
    void setId(int id){this->id =id;}
    void setName(string name){this->name = name;}
    void setPwd(string pwd){this->password =pwd;}
    void setState(string state){this->state = state;}

    //获取成员变量方法
    int getId(){return this->id ;}
    string getName(){return this->name ;}
    string getPwd(){return this->password ;}
    string getState(){return this->state ;}

    private:
    int id;
    string name;
    string password;
    string  state;

};

#endif