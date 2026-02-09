#include "sort.h"

void swap(int *a, int *b) {
	int temp = *a;
	*a = *b;
	*b = temp;
}

void sort(int *arr, int size, int *iterations) {
	for(int i = 0; i < size-1; i++) {
		if(*(arr+i) > *(arr+i+1)) {
			(*iterations)++;
			swap(arr+i, arr+i+1);
		}
	}

	(*iterations)--;
}

void fltSwap(float *a, float *b) {
	int temp = *a;
	*a = *b;
	*b = temp;
}

void fltSort(float *arr, int size, int *iterations) {
	for(int i = 0; i < size-1; i++) {
		if(*(arr+i) > *(arr+i+1)) {
			(*iterations)++;
			fltSwap(arr+i, arr+i+1);
		}
	}

	(*iterations)--;
}

