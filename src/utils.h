#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

inline double degrees(double radians)
{
	return radians * (180.0 / M_PI);
}

inline double radians(double degrees)
{
	return degrees * (M_PI / 180.0);
}
unsigned char *read_binary_file(const char *filename, size_t *size)
{
	FILE *file = fopen(filename, "rb"); // Open file in binary mode
	unsigned char *buffer = nullptr;

	if (file == nullptr) {
		perror("Error opening file");
		exit(1);
	}

	// Seek to the end of the file to get the size
	fseek(file, 0, SEEK_END);
	*size = ftell(file); // Get the size of the file
	rewind(file); // Go back to the beginning of the file

	// Allocate memory for the buffer
	buffer = malloc(*size * sizeof(unsigned char));
	if (buffer == nullptr) {
		perror("Error allocating memory");
		fclose(file);
		exit(1);
	}

	// Read the file into the buffer
	size_t read_size = fread(buffer, 1, *size, file);
	if (read_size != *size) {
		perror("Error reading file");
		free(buffer);
		fclose(file);
		exit(1);
	}

	fclose(file); // Close the file
	return buffer;
}
