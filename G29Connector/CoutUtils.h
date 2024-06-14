#pragma once
#include <string>
using namespace std;

string CinWithDefaultString(string question, string defaultAnswer) {
	string answer;
	cout << question << "(" << defaultAnswer << "):";
	cin >> answer;
	if (answer.empty())return defaultAnswer;
	return answer;
}