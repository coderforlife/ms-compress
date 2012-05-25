#include "../stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "binary_trees.h"
/*
Tree *make_empty(Tree *t)
{
	if (t != NULL)
	{
		make_empty(t->left);
		make_empty(t->right);
		free(t);
	}

	return NULL;
}

Tree *find_min(Tree *t)
{
	if (t == NULL)
	{
		return NULL;
	}
	else
		if (t->left == NULL)
		{
			return t;
		}
		else
		{
			return find_min(t->left);
		}
}

Tree *find_max(Tree *t)
{
	if (t == NULL)
	{
		return NULL;
	}
	else
		if (t->right == NULL)
		{
			return t;
		}
		else
		{
			return find_max(t->right);
		}
}

Tree *find(int elem, Tree *t)
{
	if (t == NULL)
	{
		return NULL;
	}

	if (elem < t->element)
	{
		return find(elem, t->left);
	}
	else
		if (elem > t->element)
		{
			return find(elem, t->right);
		}
		else
		{
			return t;
		}
}

//Insert i into the tree t, duplicate will be discarded
//Return a pointer to the resulting tree.
Tree * insert(int value, Tree * t)
{
	Tree * new_node;

	if (t == NULL)
	{
		new_node = (Tree *) malloc (sizeof (Tree));
		if (new_node == NULL)
		{
			return t;
		}

		new_node->element = value;

		new_node->left = new_node->right = NULL;
		return new_node;
	}

	if (value < t->element)
	{
		t->left = insert(value, t->left);
	}
	else
		if (value > t->element)
		{
			t->right = insert(value, t->right);
		}
		else
		{
			//duplicate, ignore it
			return t;
		}
	return t;
}

Tree * delete(int value, Tree * t)
{
	//Deletes node from the tree
	// Return a pointer to the resulting tree
	Tree * x;
	Tree *tmp_cell;

	if (t==NULL)
		return NULL;

	if (value < t->element)
	{
		t->left = delete(value, t->left);
	}
	else
		if (value > t->element)
		{
			t->right = delete(value, t->right);
		}
		else
			if (t->left && t->right)
			{
				tmp_cell = find_min(t->right);
				t->element = tmp_cell->element;
				t->right = delete(t->element, t->right);
			}
			else
			{
				tmp_cell = t;
				if (t->left == NULL)
					t = t->right;
				else
					if (t->right == NULL)
						t = t->left;
				free(tmp_cell);
			}

	return t;
}

//driver routine
int main()
{
	//A sample use of these functions.  Start with the empty tree
	//insert some stuff into it, and then delete it
	Tree * root;
	int i;
	root = NULL;

	make_empty(root);

	printf("\nAfter inserting element 10..\n");
	root = insert(10, root);
	print_ascii_tree(root);

	printf("\nAfter inserting element 5..\n");
	root = insert(5, root);
	print_ascii_tree(root);

	printf("\nAfter inserting element 15..\n");
	root = insert(15, root);
	print_ascii_tree(root);

	printf("\nAfter inserting elements 9, 13..\n");
	root = insert(9, root);
	root = insert(13, root);
	print_ascii_tree(root);

	printf("\nAfter inserting elements 2, 6, 12, 14, ..\n");
	root = insert(2, root);
	root = insert(6, root);
	root = insert(12, root);
	root = insert(14, root);
	print_ascii_tree(root);

	printf("\n\nAfter deleting a node (14) with no child..\n");
	root = delete(14, root);
	print_ascii_tree(root);

	printf("\n\nAfter deleting a node (13) with left child..\n");
	root = delete(13, root);
	print_ascii_tree(root);

	printf("\n\nAfter deleting a node (5) with left and right children..\n");
	root = delete(5, root);
	print_ascii_tree(root);

	printf("\n\nAfter deleting a node (10, root node) with left and right children..\n");
	root = delete(10, root);
	print_ascii_tree(root);

	make_empty(root);
}
*/