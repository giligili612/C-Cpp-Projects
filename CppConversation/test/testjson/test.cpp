#include "json.hpp"
using json = nlohmann::json;

#include <iostream>
#include <string>
using namespace std;

string func1()
{
    json js;
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"] = "hello, what are you doing now?";
    string sendBuf = js.dump();
    //cout << sendBuf.c_str() << endl;
    return sendBuf;
}

string func2()
{
    json js;
    js["id"] = {1,2,3,4,5};
    js["name"] = "zhang san";
    js["msg"]["zhang san"] = "Hello World";
    js["msg"]["liu shuo"] = "hello china";
    js["msg"] = {{"zhang san", "Hello World"}, {"liu shuo", "hello china"}};
    //cout << js << endl;
    return js.dump();
}

int main()
{
    string buf = func2();
    json js = json::parse(buf);
    // cout << js["msg_type"] << endl;
    // cout << js["from"] << endl;
    // cout << js["to"] << endl;
    // cout << js["msg"]  << endl;
    //func2();
    cout << js["id"] << endl;
    auto a = js["id"];
    cout << a[2] << endl;
    auto msgjs = js["msg"];
    cout << msgjs["liu shuo"] << endl;
    cout << msgjs["zhang san"] << endl;
    return 0;
}
