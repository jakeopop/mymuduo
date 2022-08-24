#include <iostream>
#include <string>

using namespace std;

template <class T>
string get_type_name()
{
    string s = __PRETTY_FUNCTION__;
    auto pos = s.find("T = "); 
    pos += 4;
    auto pos2 = s.find_first_of(";]", pos);
    return s.substr(pos, pos2 - pos);
}

int main()
{
    cout << get_type_name<uint64_t>() << endl;

    return 0;
}