#ifndef PARSER_H
#define PARSER_H

#include "dynamicstring.h"
#include "jobcontrol.h"

struct SepNode{
	char *cmd;
	char sync;
	struct SepNode *next;
};

struct SepNode *parse_sep(const char *str);
void free_sep_list(struct SepNode *head);

enum LogicType {
	LOGIC_AND,
	LOGIC_OR,
	LOGIC_NONE
};

struct LogicNode{
	char *cmd;
	enum LogicType type;
	struct LogicNode *next;
};

struct LogicNode *parse_logic(const char *str);
void free_logic_list(struct LogicNode *head);

struct job *parse_job(char *str);

#endif