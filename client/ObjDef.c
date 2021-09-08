#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "Gamestate.h"

#define BALANCE(t) ((((t)->child[0]) ? (t)->child[0]->height : 0) - (((t)->child[1]) ? (t)->child[1]->height : 0))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
objDef* objDefRoot = NULL;
void objDefTest(int draw);
void objDefInit(){
	#ifndef NDEBUG
		objDefTest(0);
	#endif
}
void objDefFree(objDef* t){
	if(t->otype == 'o'){
		free(t->odat.name);
	}
	free(t);
}
void getNewHeight(objDef* t){
	int lh = (t->child[0]) ? t->child[0]->height : 0;
	int rh = (t->child[1]) ? t->child[1]->height : 0;
	t->height = MAX(lh, rh)+1;
}
#ifndef NDEBUG
void validateTreeRec(objDef* t){
	if(!t) return;
	int preheight = t->height;
	getNewHeight(t);
	//printf("%d %d\n", preheight, t->height);
	assert(preheight == t->height);
	assert(abs(BALANCE(t)) < 2);
	assert(t->child[0] == NULL || t->child[0]->id < t->id);
	assert(t->child[1] == NULL || t->child[1]->id > t->id);
	for(int idx = 0; idx < 2; idx++){
		assert(t->child[idx] == NULL || t->child[idx]->parent == t);
		validateTreeRec(t->child[idx]);
	}
}
#endif
int validateTree(){
	#ifndef NDEBUG
	validateTreeRec(objDefRoot);
	#endif
	return 1;
}
objDef* objDefNext(objDef* t){
	objDef* res = t;
	if(!(t->child[1])){//if we dont have a right child, go up till we are bigger than the target
		while(res->id <= t->id){
			res = res->parent;
			if(!res) return NULL;
		}
	}else{//otherwise, go right one, then go left to the bottom
		res = t->child[1];
		while(res->child[0]){
			res = res->child[0];
		}
	}
	return res;
}
objDef* objDefFirst(){
	objDef* res = objDefRoot;
	if(!res) return NULL;
	while(res->child[0]){
		res = res->child[0];
	}
	assert(res);
	return res;
}
objDef* objDefGet(int id){
	objDef* ret = objDefRoot;
	while(ret != NULL){
		if(ret->id < id){
			ret = ret->child[1];
		}else if(ret->id > id){
			ret = ret->child[0];
		}else{
			break;
		}
	}
	assert(!ret || ret->id == id);
	return ret;
}
void objDefAgeAll(){
	objDef* cond = objDefFirst();
	while(cond){
		cond->age++;
		cond = objDefNext(cond);
	}
}
void objDefDeleteOld(int age){
	objDef* cond = objDefFirst();
	//prev is where we start back after deleting our current one
	objDef* prev = NULL;
	while(cond){
		if(cond->age >= age){
			objDefDelete(cond);
			if(prev != NULL){
				cond = prev;
			}else{
				cond = NULL;
			}
		}
		if(cond){
			prev = cond;
			cond = objDefNext(cond);
		}else{
			cond = objDefFirst();
		}
	}
}
objDef* rotate(objDef* t, int side){
	int otherside = (side+1)%2;
	objDef* tmp = t->child[otherside];
	//move tmp's left child to t's right child position
	t->child[otherside] = tmp->child[side];
	if(t->child[otherside]) t->child[otherside]->parent = t;
	//make tmp's left child t
	tmp->child[side] = t;
	tmp->parent = t->parent;
	t->parent = tmp;
	//update heights
	getNewHeight(t);
	getNewHeight(tmp);
	if(tmp->parent){//fix the pointers to this rotated tree
		if(tmp->parent->id < tmp->id){//FIXME this shouldnt be a conditional
			tmp->parent->child[1] = tmp;
		}else{
			tmp->parent->child[0] = tmp;
		}
	}else objDefRoot = tmp;
	return tmp;
}

objDef* tryRotate(objDef* t){
	int balance = BALANCE(t);
	if(balance < -1){
		if(BALANCE(t->child[1]) > 0){
			rotate(t->child[1], 1);
		}
		return rotate(t, 0);
	}else if(balance > 1){
		if(BALANCE(t->child[0]) < 0){
			rotate(t->child[0], 0);
		}
		return rotate(t, 1);
	}else{
		return t;
	}
}
objDef* objDefInsert(int id){
	objDef* n = malloc(sizeof(objDef));
	n->child[0] = NULL;
	n->child[1] = NULL;
	n->parent = NULL;
	n->height = 1;
	n->id = id;
	n->pending = WAITING;
	n->revision = -1;
	n->otype = 0;
	n->age = 0;
	if(objDefRoot == NULL){
		objDefRoot = n;
	}else{
		objDef* conductor = objDefRoot;
		while(1){//go down to insert
			objDef** next;
			assert(id != conductor->id);
			if(id < conductor->id){
				next = &(conductor->child[0]);
			}else{
				next = &(conductor->child[1]);
			}
			if(*next == NULL){//we reached unused node
				*next = n;
				n->parent = conductor;
				break;
			}else{
				conductor = *next;
			}
		}
		conductor = n;
		while(conductor->parent != NULL){//go back up rotating as needed
			int minHeight = conductor->height+1;
			conductor = conductor->parent;
			if(conductor->height < minHeight) conductor->height = minHeight;
			conductor = tryRotate(conductor);
		}
	}
	assert(validateTree(objDefRoot));
	return n;
}
void objDefDelete(objDef* target){
	objDef* repair;
	if(target->child[0]){
		if(target->child[1]){//both children
			//objDefNext is guaranteed to return an element in target's subtree in this case.
			objDef* replace = objDefNext(target);
			//we need to start repairing at the parent of where we pulled our replacement from (unless we are deleting that node)
			if(replace->parent == target){
				repair = replace;
			}else{
				repair = replace->parent;
			}
			//replace is known to have at most a single right child
			if(replace->parent->id < replace->id){
				replace->parent->child[1] = replace->child[1];
			}else{
				replace->parent->child[0] = replace->child[1];
			}
			if(replace->child[1]){
				replace->child[1]->parent = replace->parent;
			}
			replace->parent = target->parent;
			for(int idx = 0; idx < 2; idx++){
				replace->child[idx] = target->child[idx];
				if(replace->child[idx]) replace->child[idx]->parent = replace;
			}
			if(replace->parent){
				if(replace->id < replace->parent->id){
					replace->parent->child[0] = replace;
				}else{
					replace->parent->child[1] = replace;
				}
			}else objDefRoot = replace;
		}else{//left child only
			repair = target->parent;
			target->child[0]->parent = repair;
			if(repair){
				if(repair->id < target->id){//FIXME this shouldnt be a conditional
					repair->child[1] = target->child[0];
				}else{
					repair->child[0] = target->child[0];
				}
			}else objDefRoot = target->child[0];
		}
	}else{
		if(target->child[1]){//right child only
			repair = target->parent;
			target->child[1]->parent = repair;
			if(repair){
				if(repair->id < target->id){
					repair->child[1] = target->child[1];
				}else{
					repair->child[0] = target->child[1];
				}
			}else objDefRoot = target->child[1];
		}else{//no children
			repair = target->parent;
			if(repair){
				if(repair->id < target->id){
					repair->child[1] = NULL;
				}else{
					repair->child[0] = NULL;
				}
			}else objDefRoot = NULL;
		}
	}
	objDefFree(target);
	while(repair != NULL){
		getNewHeight(repair);
		repair = tryRotate(repair);
		repair = repair->parent;
	}
	assert(validateTree(objDefRoot));
}
void printTree(objDef* t, int level){
	if(!level) printf("###\n");
	if(t){
		printTree(t->child[1], level+1);
		for(int x = 0; x < level; x++) printf("\t");
		printf("%d\n", t->id);
		printTree(t->child[0], level+1);
	}
	if(!level) printf("###\n");
}
void objDefPrintTree(){
	printTree(objDefRoot, 0);
}
void objDefTest(int draw){
	for(int idx = 0; idx < 20; idx++){
		objDefInsert(idx);
		if(draw) objDefPrintTree();
		validateTree(objDefRoot);
	}
	for(int idx = 0; idx < 20; idx++){
		objDefDelete(objDefGet(idx));
		if(draw) objDefPrintTree();
		validateTree(objDefRoot);
	}
	for(int idx = 19; idx >= 0; idx--){
		objDefInsert(idx);
		if(draw) objDefPrintTree();
		validateTree(objDefRoot);
	}
	for(int idx = 19; idx >= 0; idx--){
		objDefDelete(objDefGet(idx));
		if(draw) objDefPrintTree();
		validateTree(objDefRoot);
	}
	for(int idx = 0; idx < 20; idx++){
		objDefInsert(idx);
		if(draw) objDefPrintTree();
		validateTree(objDefRoot);
	}
	objDefDelete(objDefGet(5));
	validateTree();
	objDefDelete(objDefGet(18));
	validateTree();
	objDefDelete(objDefGet(6));
	validateTree();
	objDefDelete(objDefGet(2));
	validateTree();
	objDefDelete(objDefGet(7));
	validateTree();
	objDefDelete(objDefGet(16));
	validateTree();
	objDefAgeAll();
	objDefDeleteOld(1);
	validateTree();
}
