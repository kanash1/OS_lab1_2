#pragma once

#ifndef COPY_FILE_H
#define COPY_FILE_H

#include <iostream>
#include <string>
#include <cstdint>
#include <functional>
#include <windows.h>
#include <timeapi.h>

#undef max

#include "input_helper.h"

class Filesize {
public:
	Filesize() : low_order(0), high_order(0) {}
	Filesize(const HANDLE& handle) { low_order = GetFileSize(handle, &high_order); }
	void set_size(const HANDLE& handle) { low_order = GetFileSize(handle, &high_order); }
	DWORD get_low() const { return low_order; }
	DWORD get_high() const { return high_order; }
	uint64_t get_fullsize() const { return (static_cast<uint64_t>(high_order) << 32) + static_cast<uint64_t>(low_order); } // don't work with LP64
private:
	DWORD low_order;
	DWORD high_order;
};

size_t callback_count = 0;

VOID CALLBACK FileIOCompletionRoutine(DWORD error, DWORD number_of_bytes, LPOVERLAPPED p_over) { ++callback_count; }

void action_overlapped(
	const std::function<BOOL(HANDLE, LPVOID, DWORD, LPOVERLAPPED, LPOVERLAPPED_COMPLETION_ROUTINE)>& func,
	const HANDLE& file_handle,
	const size_t& block_size,
	const size_t& operations_count,
	const Filesize& filesize,
	uint8_t**& buffer,
	OVERLAPPED*& over
) {
	size_t s_operations_count = 0;
	uint64_t current_size = filesize.get_fullsize();
	uint64_t shift = static_cast<uint64_t>(block_size) * static_cast<uint64_t>(operations_count);
	for (size_t i = 0; i < operations_count; ++i) {
		++s_operations_count;
		func(file_handle, buffer[i], static_cast<DWORD>(block_size), &over[i], FileIOCompletionRoutine);
		if (current_size <= static_cast<uint64_t>(block_size))
			break;
		current_size -= static_cast<uint64_t>(block_size);
	}
	while (callback_count < s_operations_count)
		SleepEx(std::numeric_limits<DWORD>::max(), true);
	for (size_t i = 0; i < operations_count; i++) {
		over[i].Offset += static_cast<DWORD>((shift << 32) >> 32);
		over[i].OffsetHigh += static_cast<DWORD>(shift >> 32);
	}
	callback_count = 0;
}

void copy_file(
	const HANDLE& source_handle,
	const HANDLE& copy_handle,
	const size_t& block_size,
	const size_t& operations_count,
	const Filesize& filesize,
	uint8_t**& buffer,
	OVERLAPPED*& over_read,
	OVERLAPPED*& over_write
) {
	uint64_t current_size = filesize.get_fullsize();
	uint64_t shift = static_cast<uint64_t>(block_size * operations_count);
	do {
		if (shift > current_size)
			shift = current_size;
		action_overlapped(ReadFileEx, source_handle, block_size, operations_count, filesize, buffer, over_read);
		action_overlapped(WriteFileEx, copy_handle, block_size, operations_count, filesize, buffer, over_write);
		current_size -= shift;
	} while (current_size != 0);
}

uint64_t time_calculation(const LARGE_INTEGER& start, const LARGE_INTEGER& end, const LARGE_INTEGER& freq) {
	return ((end.QuadPart - start.QuadPart) * 1000000) / freq.QuadPart;
}

void process_copy() {
	OVERLAPPED* over_read = nullptr;
	OVERLAPPED* over_write = nullptr;
	uint8_t** buffer = nullptr;	
	long tmp = 0;
	size_t block_size = 0;
	size_t operations_count = 0;
	Filesize filesize;
	HANDLE source_handle;
	HANDLE copy_handle;
	InputError error;
	std::string source_path;
	std::string copy_path;

	std::cout << "Enter source path:\n";
	std::getline(std::cin, source_path);
	source_handle = CreateFile(source_path.c_str(), GENERIC_READ, 0, nullptr,
		OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (source_handle == INVALID_HANDLE_VALUE) {
		std::cout << "Error occurred while openning the file. Error code: " << GetLastError() << '\n';
		return;
	}

	std::cout << "Enter copy path:\n";
	std::getline(std::cin, copy_path);
	copy_handle = CreateFile(copy_path.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (copy_handle == INVALID_HANDLE_VALUE && GetLastError() != 0) {
		std::cout << "Error occurred while creating the file. Error code: " << GetLastError() << '\n';
		CloseHandle(source_handle);
		return;
	}

	std::cout << "Enter block size:\n";
	error = input(std::cin, block_size);
	if (error != InputError::ERROR_NORMAL) {
		std::cout << "Invalid input\n";
		CloseHandle(source_handle);
		CloseHandle(copy_handle);
		return;
	}
	block_size *= 4096;

	std::cout << "Enter operations count:\n";
	error = input(std::cin, operations_count);
	if (error != InputError::ERROR_NORMAL) {
		std::cout << "Invalid input\n";
		CloseHandle(source_handle);
		CloseHandle(copy_handle);
		return;
	}

	filesize.set_size(source_handle);
	buffer = new uint8_t * [operations_count];
	over_read = new OVERLAPPED[operations_count];
	over_write = new OVERLAPPED[operations_count];
	for (size_t i = 0; i < operations_count; ++i) {
		uint64_t shift = static_cast<uint64_t>(i) * static_cast<uint64_t>(block_size);
		over_read[i].Offset = over_write[i].Offset = static_cast<DWORD>((shift << 32) >> 32);
		over_read[i].OffsetHigh = over_write[i].OffsetHigh = static_cast<DWORD>(shift >> 32);
		buffer[i] = new uint8_t[block_size];
	}

	LARGE_INTEGER start_time, end_time, frequency;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start_time);
	copy_file(source_handle, copy_handle, block_size, operations_count, filesize, buffer, over_read, over_write);
	QueryPerformanceCounter(&end_time);

	std::cout << "Time: " << time_calculation(start_time, end_time, frequency) << " microseconds\n";

	tmp = filesize.get_high();
	SetFilePointer(copy_handle, filesize.get_low(), &tmp, FILE_BEGIN);
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
