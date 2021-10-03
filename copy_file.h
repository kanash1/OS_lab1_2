#pragma once

#ifndef COPY_FILE_H
#define COPY_FILE_H

#include <iostream>
#include <string>
#include <functional>
#include <windows.h>
#include <timeapi.h>

#undef max

#include "input_helper.h"

size_t callback_count = 0;

VOID CALLBACK FileIOCompletionRoutine(DWORD error, DWORD number_of_bytes, LPOVERLAPPED p_over) { ++callback_count; }

void action_file_overlapped(
	const std::function<BOOL(HANDLE, LPVOID, DWORD, LPOVERLAPPED, LPOVERLAPPED_COMPLETION_ROUTINE)>& func,
	const HANDLE& file_handle,
	const size_t& block_size,
	const size_t& operations_count,
	int l_file_size,
	BYTE**& buffer,
	OVERLAPPED*& over
) {
	size_t s_operations_count = 0;
	for (size_t i = 0; i < operations_count; ++i) {
		if (l_file_size > 0) {
			++s_operations_count;
			func(file_handle, buffer[i], block_size, &over[i], FileIOCompletionRoutine);
			l_file_size -= block_size;
		}
	}
	while (callback_count < s_operations_count)
		SleepEx(SIZE_MAX, true);
	for (size_t i = 0; i < operations_count; i++)
		over[i].Offset += block_size * operations_count;
	callback_count = 0;
}

void copy_file(
	const HANDLE& source_handle,
	const HANDLE& copy_handle,
	const size_t& block_size,
	const size_t& operations_count,
	int l_file_size,
	BYTE**& buffer,
	OVERLAPPED*& over_read,
	OVERLAPPED*& over_write
) {
	int current_size = l_file_size;
	do {
		action_file_overlapped(ReadFileEx, source_handle, block_size, operations_count, l_file_size, buffer, over_read);
		action_file_overlapped(WriteFileEx, copy_handle, block_size, operations_count, l_file_size, buffer, over_write);
		current_size -= block_size * operations_count;
	} while (current_size > 0);
}

LONGLONG time_calculation(const LARGE_INTEGER& start, const LARGE_INTEGER& end, const LARGE_INTEGER& freq) {
	LARGE_INTEGER res;
	res.QuadPart = end.QuadPart - start.QuadPart;
	res.QuadPart *= 1000000;
	res.QuadPart /= freq.QuadPart;
	return res.QuadPart;
}

void process_copy() {
	std::string source_path;
	std::string copy_path;

	std::cout << "Enter source path:\n";
	std::getline(std::cin, source_path);
	HANDLE source_handle = CreateFile(source_path.c_str(), GENERIC_READ, 0, nullptr,
		OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (source_handle == INVALID_HANDLE_VALUE) {
		std::cout << "Error occurred while openning the file. Error code: " << GetLastError() << '\n';
		return;
	}

	std::cout << "Enter copy path:\n";
	std::getline(std::cin, copy_path);
	HANDLE copy_handle = CreateFile(copy_path.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (copy_handle == INVALID_HANDLE_VALUE && GetLastError() != 0) {
		std::cout << "Error occurred while creating the file. Error code: " << GetLastError() << '\n';
		CloseHandle(source_handle);
		return;
	}

	size_t block_size;
	std::cout << "Enter block size:\n";
	InputError error = input(std::cin, block_size);
	if (error != InputError::ERROR_NORMAL) {
		std::cout << "Invalid input\n";
		return;
	}
	block_size *= 4096;

	size_t operations_count;
	std::cout << "Enter operations count:\n";
	error = input(std::cin, operations_count);
	if (error != InputError::ERROR_NORMAL) {
		std::cout << "Invalid input\n";
		return;
	}

	int l_source_size = 0;
	DWORD h_source_size = 0;
	l_source_size = GetFileSize(source_handle, &h_source_size);
	BYTE** buffer = new BYTE * [operations_count];
	for (size_t i = 0; i < operations_count; ++i)
		buffer[i] = new BYTE[block_size];
	OVERLAPPED* over_read = new OVERLAPPED[operations_count];
	OVERLAPPED* over_write = new OVERLAPPED[operations_count];
	for (size_t i = 0; i < operations_count; ++i) {
		over_write[i].Offset = i * block_size;
		over_write[i].OffsetHigh = i * h_source_size;
		over_read[i].Offset = over_write[i].Offset;
		over_read[i].OffsetHigh = over_write[i].OffsetHigh;
	}

	LARGE_INTEGER start_time, end_time, frequency;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start_time);
	copy_file(source_handle, copy_handle, block_size, operations_count, l_source_size, buffer, over_read, over_write);
	QueryPerformanceCounter(&end_time);

	std::cout << time_calculation(start_time, end_time, frequency) << '\n';

	SetFilePointer(copy_handle, l_source_size, nullptr, FILE_BEGIN);
	SetEndOfFile(copy_handle);
	CloseHandle(source_handle);
	CloseHandle(copy_handle);

	for (size_t i = 0; i < operations_count; ++i)
		delete[] buffer[i];
	delete[] buffer;
	delete[] over_read;
	delete[] over_write;
}

#endif // !COPY_FILE_H
