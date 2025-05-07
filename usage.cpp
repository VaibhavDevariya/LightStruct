#include <iostream>
#include "LightStruct.h"

int main()
{
	LightStruct ls = LightStruct::fromFile("example.lsf");

	auto age = ls["age"].asInt();
	auto name = ls["name"].asStr();

	std::cout << age << "\t" << name<<"\n";

	auto age1 = ls.getInt("age");
	auto name1 = ls.getString("name");

	std::cout << age << "\t" << name;

	auto str = ls.serialize();
	std::cout << "--------------------\n";
	std::cout << str << "\n";
	std::cout << "--------------------\n";
}