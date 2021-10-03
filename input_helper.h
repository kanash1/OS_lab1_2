#pragma once

#ifndef INPUT_HELPER_H
#define INPUT_HELPER_H
#include <iostream>

enum class InputError {
	ERROR_NORMAL,
	ERROR_INVALID,
	ERROR_TRASH,
	ERROR_NEL
};

void trashcleaner(std::istream&);
void processFail(std::istream&, InputError&);
void nelhater(std::istream&, InputError&);
void trashhater(std::istream&, InputError&);

template <typename T>
InputError input(std::istream& is, T& value) {
	InputError input_error = InputError::ERROR_NORMAL;
	is.clear();
	nelhater(is, input_error);
	is >> value;
	trashhater(is, input_error);
	processFail(is, input_error);
	trashcleaner(is);
	return input_error;
}

#endif // !INPUT_HELPER_H