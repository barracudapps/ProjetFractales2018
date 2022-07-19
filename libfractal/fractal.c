#include <stdlib.h>
#include "fractal.h"
#include <stdio.h>



struct fractal *fractal_new(const char *name, int width, int height, double a, double b)
{
    struct fractal * f = (struct fractal *) malloc(sizeof(struct fractal));
    if(f == NULL)
    {
		return NULL;
	}
	f->name = (char*) malloc(64*sizeof(char));
	int i = 0;
    while(name[i] != '\0')
    {
		f->name[i] = name[i];
		i++;
	}
	f->name[i] = '\0';
    f->width = width;
    f->height = height;
    f->a = a;
    f->b = b;
    f->val = 0;
    f->value = (int**) malloc(height * sizeof(int*));
    if(f->value == NULL)
    {
		free(f);
		return NULL;
	}
	int* tab = (int*) malloc(width * height * sizeof(int));
	if(tab == NULL)
    {
		free(f->value);
		free(f);
		return NULL;
	}

	f->value[0] = tab;
	for(i = 1; i<height; i++)
	{
		f->value[i] = f->value[i-1] + width;
	}
    return f;
}

void fractal_free(struct fractal *f)
{
	free(f->name);
    free(f->value[0]);
    free(f->value);
    free(f);
}

const char *fractal_get_name(const struct fractal *f)
{
    return f->name;
}

int fractal_get_value(const struct fractal *f, int x, int y)
{
    return f->value[y][x];
}

void fractal_set_value(struct fractal *f, int x, int y, int val)
{
    f->value[y][x] = val;
}

int fractal_get_width(const struct fractal *f)
{
    return f->width;
}

int fractal_get_height(const struct fractal *f)
{
    return f->height;
}

double fractal_get_a(const struct fractal *f)
{
    return f->a;
}

double fractal_get_b(const struct fractal *f)
{
    return f->b;
}
