#include "input_helper.h"

void trashcleaner(std::istream& is) {
	is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void processFail(std::istream& is, InputError& input_error) {
	if (is.fail()) {
		if (input_error == InputError::ERROR_NORMAL)
			input_error = InputError::ERROR_INVALID;
		is.clear();
	}
}

void nelhater(std::istream& is, InputError& input_error) {
	if (is.peek() == '\n') {
		is.setstate(is.failbit);
		input_error = InputError::ERROR_NEL;
	}
}

void trashhater(std::istream& is, InputError& input_error) {
	if (is.peek() != '\n' && is.peek() != EOF) {
		is.setstate(is.failbit);
		input_error = InputError::ERROR_TRASH;
	}
}