#define PRINTABLE

/*typedef struct _NODE
{
	struct _NODE* parent;
	struct _NODE* child[2];
	size_t count;
	uint_fast16_t symbol;
} NODE;
typedef NODE TREE_NODE;*/

typedef struct _x {
	uint_fast16_t symbol;
	struct _x *child[2];
} x;
typedef x TREE_NODE;