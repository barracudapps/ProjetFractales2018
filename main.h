
struct node
{
	struct fractal* f;
	struct node* next;
};
struct nodeName //structure pour stocker les noms des fractales
{
	char* s;
	struct nodeName* next;
};

void push_stackFrac(struct fractal* f,struct node** first);
struct fractal* pop_stackFrac(struct node** first);

void concate(char* s1, char* s2, char* result);
int compare(char* s1, char* s2);
