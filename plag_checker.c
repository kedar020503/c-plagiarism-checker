#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

typedef enum {
    TOKEN_KEYWORD, TOKEN_IDENTIFIER, TOKEN_LITERAL_INT, TOKEN_LITERAL_FLOAT,TOKEN_LITERAL_STRING, TOKEN_OPERATOR, TOKEN_PUNCTUATOR, TOKEN_EOF, TOKEN_UNKNOWN
} TokenCategory;

typedef struct {
    TokenCategory category;
    char *text_value;
    int line_number, column_number;
} LexicalToken;

typedef struct {
    char *source_code;
    size_t current_position, total_length;
    int current_line, current_column;
} LexicalScanner;

typedef enum {
    NODE_PROGRAM, NODE_FUNCTION, NODE_BLOCK, NODE_VAR_DECL,NODE_IF, NODE_WHILE, NODE_FOR, NODE_RETURN,NODE_EXPR_STMT, NODE_BINARY_EXPR, NODE_ASSIGN_EXPR,NODE_CALL_EXPR, NODE_IDENTIFIER, NODE_LITERAL
} SyntaxNodeType;

typedef struct SyntaxNode {
    SyntaxNodeType node_type;
    char *associated_value;
    struct SyntaxNode **child_nodes;
    int child_node_count;
    int file_size_bound;
    int source_line;
} SyntaxNode;

typedef struct {
    LexicalToken **token_list;
    int total_tokens;
    int current_token_index;
    int source_file_length;
} SyntaxParser;

typedef struct ControlFlowBlock { 
    int block_id; 
    SyntaxNode **statement_nodes;
    int statement_count;
    struct ControlFlowBlock *successor_blocks[2];
    int successor_count;
} ControlFlowBlock;

typedef struct {
    ControlFlowBlock **all_blocks;
    int total_blocks;
    int source_file_length;
} ControlFlowGraph;

typedef struct {
    int start_line_file1, end_line_file1;
    int start_line_file2, end_line_file2;
} MatchedCodeRegion;

typedef struct {
    double token_similarity_score;
    double structural_ast_score;
    double control_flow_cfg_score;
    double overall_final_score;
    MatchedCodeRegion *matched_regions;
    int total_matches_found;
} SimilarityAnalysisResult;

typedef struct {
    unsigned int hash_value;
    int start_line, end_line;
} CodeFingerprint;






char *load_file_content_to_string(const char *file_path);
LexicalScanner *initialize_lexical_scanner(char *source_code);
int tokenize_source_code(LexicalScanner *scanner, LexicalToken **output_tokens);
void release_lexical_token(LexicalToken *token);
SyntaxNode *create_syntax_node(SyntaxNodeType type, int line, int file_length);
void append_child_syntax_node(SyntaxNode *parent, SyntaxNode *child);
void recursively_release_ast(SyntaxNode *root_node);
SyntaxNode *create_deep_copy_of_ast(SyntaxNode *original_node);
SyntaxParser *initialize_syntax_parser(LexicalToken **tokens, int token_count, int file_length);
SyntaxNode *parse_syntax_tree(SyntaxParser *parser);
ControlFlowGraph *construct_control_flow_graph(SyntaxNode *function_node, int file_length);
void release_control_flow_graph(ControlFlowGraph *graph);
SimilarityAnalysisResult analyze_file_similarity(const char *path1, const char *path2);






char *load_file_content_to_string(const char *file_path) {
    FILE *file_handle = fopen(file_path, "rb");
    if (!file_handle) return NULL;
    fseek(file_handle, 0, SEEK_END);
    long file_size = ftell(file_handle);
    fseek(file_handle, 0, SEEK_SET);
    char *content_buffer = malloc(file_size + 1);
    fread(content_buffer, 1, file_size, file_handle);
    content_buffer[file_size] = '\0';
    fclose(file_handle);
    return content_buffer;
}






const char *c_keywords_list[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "int", "long", "register", "return", "short", "signed", "sizeof", "static",
    "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while"
};

static bool is_string_a_c_keyword(const char *candidate) {
    for (size_t i = 0; i < sizeof(c_keywords_list) / sizeof(char *); i++)
        if (strcmp(candidate, c_keywords_list[i]) == 0) return true;
    return false;
}

LexicalScanner *initialize_lexical_scanner(char *source_code) {
    LexicalScanner *scanner = calloc(1, sizeof(LexicalScanner));
    scanner->source_code = source_code;
    scanner->total_length = strlen(source_code);
    scanner->current_line = scanner->current_column = 1;
    return scanner;
}

static char peek_current_char(LexicalScanner *scanner) {
    return scanner->current_position >= scanner->total_length ? '\0' : scanner->source_code[scanner->current_position];
}

static char peek_following_char(LexicalScanner *scanner) {
    return scanner->current_position + 1 >= scanner->total_length ? '\0' : scanner->source_code[scanner->current_position + 1];
}

static char consume_and_advance_char(LexicalScanner *scanner) {
    char current = peek_current_char(scanner);
    scanner->current_position++;
    if (current == '\n') {
        scanner->current_line++;
        scanner->current_column = 1;
    } else {
        scanner->current_column++;
    }
    return current;
}

static void skip_unnecessary_whitespace_and_comments(LexicalScanner *scanner) {
    while (1) {
        char current = peek_current_char(scanner);
        if (isspace(current)) {
            consume_and_advance_char(scanner);
        } else if (current == '/' && peek_following_char(scanner) == '/') {
            while (peek_current_char(scanner) != '\n' && peek_current_char(scanner) != '\0')
                consume_and_advance_char(scanner);
        } else if (current == '/' && peek_following_char(scanner) == '*') {
            consume_and_advance_char(scanner);
            consume_and_advance_char(scanner);
            while (!(peek_current_char(scanner) == '*' && peek_following_char(scanner) == '/') && peek_current_char(scanner))
                consume_and_advance_char(scanner);
            if (peek_current_char(scanner)) {
                consume_and_advance_char(scanner);
                consume_and_advance_char(scanner);
            }
        } else {
            break;
        }
    }
}

static LexicalToken *create_new_token(TokenCategory category, char *text, int line, int column) {
    LexicalToken *token = malloc(sizeof(LexicalToken));
    token->category = category;
    token->text_value = text;
    token->line_number = line;
    token->column_number = column;
    return token;
}

void release_lexical_token(LexicalToken *token) {
    free(token->text_value);
    free(token);
}

LexicalToken *scan_for_next_token(LexicalScanner *scanner) {
    skip_unnecessary_whitespace_and_comments(scanner);
    int starting_line = scanner->current_line;
    int starting_column = scanner->current_column;
    char current = peek_current_char(scanner);
    
    if (!current) return create_new_token(TOKEN_EOF, strdup("EOF"), starting_line, starting_column);
    

    if (isalpha(current) || current == '_') {
        char buffer[scanner->total_length + 1];
        int buffer_index = 0;
        while (isalnum(peek_current_char(scanner)) || peek_current_char(scanner) == '_')
            buffer[buffer_index++] = consume_and_advance_char(scanner);
        buffer[buffer_index] = '\0';
        TokenCategory cat = is_string_a_c_keyword(buffer) ? TOKEN_KEYWORD : TOKEN_IDENTIFIER;
        return create_new_token(cat, strdup(buffer), starting_line, starting_column);
    }
    

    if (isdigit(current)) {
        char buffer[scanner->total_length + 1];
        int buffer_index = 0;
        bool has_decimal_point = false;
        while (isdigit(peek_current_char(scanner)) || peek_current_char(scanner) == '.') {
            if (peek_current_char(scanner) == '.') has_decimal_point = true;
            buffer[buffer_index++] = consume_and_advance_char(scanner);
        }
        buffer[buffer_index] = '\0';
        TokenCategory cat = has_decimal_point ? TOKEN_LITERAL_FLOAT : TOKEN_LITERAL_INT;
        return create_new_token(cat, strdup(buffer), starting_line, starting_column);
    }

    if (current == '"') {
        consume_and_advance_char(scanner);
        char buffer[scanner->total_length + 1];
        int buffer_index = 0;
        while (peek_current_char(scanner) != '"' && peek_current_char(scanner)) {
            if (peek_current_char(scanner) == '\\') buffer[buffer_index++] = consume_and_advance_char(scanner);
            buffer[buffer_index++] = consume_and_advance_char(scanner);
        }
        if (peek_current_char(scanner) == '"') consume_and_advance_char(scanner);
        buffer[buffer_index] = '\0';
        return create_new_token(TOKEN_LITERAL_STRING, strdup(buffer), starting_line, starting_column);
    }
    

    const char *operators_2char[] = {"++", "--", "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "&&", "||", "->"};
    for (size_t i = 0; i < sizeof(operators_2char) / sizeof(char *); i++) {
        if (current == operators_2char[i][0] && peek_following_char(scanner) == operators_2char[i][1]) {
            consume_and_advance_char(scanner);
            consume_and_advance_char(scanner);
            return create_new_token(TOKEN_OPERATOR, strdup(operators_2char[i]), starting_line, starting_column);
        }
    }
    

    if (strchr("(){}[],;:. ", consume_and_advance_char(scanner))) {
        char text_rep[2] = {current, '\0'};
        TokenCategory cat = strchr("(){}[],;:.", current) ? TOKEN_PUNCTUATOR : TOKEN_OPERATOR;
        return create_new_token(cat, strdup(text_rep), starting_line, starting_column);
    }
    
    char unknown_rep[2] = {current, '\0'};
    return create_new_token(TOKEN_UNKNOWN, strdup(unknown_rep), starting_line, starting_column);
}

int tokenize_source_code(LexicalScanner *scanner, LexicalToken **output_tokens) {
    int token_count = 0;
    LexicalToken *current_token;
    do {
        current_token = scan_for_next_token(scanner);
        output_tokens[token_count++] = current_token;
    } while (current_token->category != TOKEN_EOF);
    return token_count;
}






SyntaxNode *create_syntax_node(SyntaxNodeType type, int line, int file_length) {
    SyntaxNode *node = calloc(1, sizeof(SyntaxNode));
    node->node_type = type;
    node->source_line = line;
    node->file_size_bound = file_length;
    node->child_nodes = calloc(file_length + 1, sizeof(SyntaxNode *));
    return node;
}

void append_child_syntax_node(SyntaxNode *parent, SyntaxNode *child) {
    if (parent && child) parent->child_nodes[parent->child_node_count++] = child;
}

void recursively_release_ast(SyntaxNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_node_count; i++) recursively_release_ast(node->child_nodes[i]);
    free(node->child_nodes);
    free(node->associated_value);
    free(node);
}

SyntaxNode *create_deep_copy_of_ast(SyntaxNode *original) {
    if (!original) return NULL;
    SyntaxNode *copy = create_syntax_node(original->node_type, original->source_line, original->file_size_bound);
    if (original->associated_value) copy->associated_value = strdup(original->associated_value);
    for (int i = 0; i < original->child_node_count; i++)
        append_child_syntax_node(copy, create_deep_copy_of_ast(original->child_nodes[i]));
    return copy;
}







SyntaxParser *initialize_syntax_parser(LexicalToken **tokens, int token_count, int file_length) {
    SyntaxParser *parser = malloc(sizeof(SyntaxParser));
    parser->token_list = tokens;
    parser->total_tokens = token_count;
    parser->current_token_index = 0;
    parser->source_file_length = file_length;
    return parser;
}

static LexicalToken *parser_lookahead(SyntaxParser *parser) {
    return parser->current_token_index >= parser->total_tokens ? NULL : parser->token_list[parser->current_token_index];
}

static LexicalToken *parser_advance_token(SyntaxParser *parser) {
    LexicalToken *token = parser_lookahead(parser);
    if (token) parser->current_token_index++;
    return token;
}

static bool parser_check_current_type(SyntaxParser *parser, TokenCategory type, const char *optional_text) {
    LexicalToken *token = parser_lookahead(parser);
    return token && token->category == type && (!optional_text || !strcmp(token->text_value, optional_text));
}

static LexicalToken *parser_match_and_consume(SyntaxParser *parser, TokenCategory type, const char *optional_text) {
    return parser_check_current_type(parser, type, optional_text) ? parser_advance_token(parser) : NULL;
}

static SyntaxNode *parse_expression(SyntaxParser *parser);
static SyntaxNode *parse_statement(SyntaxParser *parser);

static SyntaxNode *parse_primary_expression(SyntaxParser *parser) {
    LexicalToken *token;
    if ((token = parser_match_and_consume(parser, TOKEN_LITERAL_INT, NULL)) ||
        (token = parser_match_and_consume(parser, TOKEN_LITERAL_FLOAT, NULL)) ||
        (token = parser_match_and_consume(parser, TOKEN_LITERAL_STRING, NULL))) {
        SyntaxNode *node = create_syntax_node(NODE_LITERAL, token->line_number, parser->source_file_length);
        node->associated_value = strdup(token->text_value);
        return node;
    }
    
    if ((token = parser_match_and_consume(parser, TOKEN_IDENTIFIER, NULL))) {
        SyntaxNode *identifier_node = create_syntax_node(NODE_IDENTIFIER, token->line_number, parser->source_file_length);
        identifier_node->associated_value = strdup(token->text_value);
        
        // Handle function calls
        if (parser_check_current_type(parser, TOKEN_PUNCTUATOR, "(")) {
            SyntaxNode *call_node = create_syntax_node(NODE_CALL_EXPR, token->line_number, parser->source_file_length);
            append_child_syntax_node(call_node, identifier_node);
            parser_match_and_consume(parser, TOKEN_PUNCTUATOR, "(");
            while (!parser_check_current_type(parser, TOKEN_PUNCTUATOR, ")")) {
                SyntaxNode *argument = parse_expression(parser);
                if (argument) append_child_syntax_node(call_node, argument);
                if (!parser_match_and_consume(parser, TOKEN_PUNCTUATOR, ",")) break;
            }
            parser_match_and_consume(parser, TOKEN_PUNCTUATOR, ")");
            return call_node;
        }
        return identifier_node;
    }
    
    if (parser_match_and_consume(parser, TOKEN_PUNCTUATOR, "(")) {
        SyntaxNode *nested = parse_expression(parser);
        parser_match_and_consume(parser, TOKEN_PUNCTUATOR, ")");
        return nested;
    }
    return NULL;
}

static SyntaxNode *parse_expression(SyntaxParser *parser) {
    SyntaxNode *left_operand = parse_primary_expression(parser);
    if (!left_operand) return NULL;
    
    LexicalToken *operator_token = parser_lookahead(parser);
    if (operator_token && operator_token->category == TOKEN_OPERATOR) {
        // Handle assignments
        if (strchr("=+-*/", operator_token->text_value[0]) && (operator_token->text_value[1] == '=' || operator_token->text_value[1] == '\0')) {
            bool is_compound_assignment = (operator_token->text_value[1] == '=');
            parser_advance_token(parser);
            SyntaxNode *assignment_node = create_syntax_node(NODE_ASSIGN_EXPR, operator_token->line_number, parser->source_file_length);
            append_child_syntax_node(assignment_node, left_operand);
            SyntaxNode *right_operand = parse_expression(parser);
            
            if (is_compound_assignment) {
                // Normalize compound assignments: a += b -> a = a + b
                SyntaxNode *normalized_binary = create_syntax_node(NODE_BINARY_EXPR, operator_token->line_number, parser->source_file_length);
                char simple_op[2] = {operator_token->text_value[0], '\0'};
                normalized_binary->associated_value = strdup(simple_op);
                append_child_syntax_node(normalized_binary, create_deep_copy_of_ast(left_operand));
                if (right_operand) append_child_syntax_node(normalized_binary, right_operand);
                append_child_syntax_node(assignment_node, normalized_binary);
            } else if (right_operand) {
                append_child_syntax_node(assignment_node, right_operand);
            }
            return assignment_node;
        }
        
        // Handle generic binary expressions
        parser_advance_token(parser);
        SyntaxNode *binary_node = create_syntax_node(NODE_BINARY_EXPR, operator_token->line_number, parser->source_file_length);
        binary_node->associated_value = strdup(operator_token->text_value);
        append_child_syntax_node(binary_node, left_operand);
        SyntaxNode *right_side = parse_expression(parser);
        if (right_side) append_child_syntax_node(binary_node, right_side);
        return binary_node;
    }
    return left_operand;
}

static SyntaxNode *parse_block_statement(SyntaxParser *parser) {
    LexicalToken *brace_token = parser_match_and_consume(parser, TOKEN_PUNCTUATOR, "{");
    if (!brace_token) return NULL;
    
    SyntaxNode *block_node = create_syntax_node(NODE_BLOCK, brace_token->line_number, parser->source_file_length);
    while (!parser_check_current_type(parser, TOKEN_PUNCTUATOR, "}")) {
        SyntaxNode *internal_statement = parse_statement(parser);
        if (internal_statement) append_child_syntax_node(block_node, internal_statement);
        else if (parser_lookahead(parser) && parser_lookahead(parser)->category == TOKEN_EOF) break;
        else parser_advance_token(parser);
    }
    parser_match_and_consume(parser, TOKEN_PUNCTUATOR, "}");
    return block_node;
}

static SyntaxNode *parse_statement(SyntaxParser *parser) {
    LexicalToken *current_token = parser_lookahead(parser);
    if (!current_token) return NULL;
    
    if (parser_check_current_type(parser, TOKEN_PUNCTUATOR, "{")) return parse_block_statement(parser);
    
    if (parser_match_and_consume(parser, TOKEN_KEYWORD, "if")) {
        SyntaxNode *if_node = create_syntax_node(NODE_IF, current_token->line_number, parser->source_file_length);
        parser_match_and_consume(parser, TOKEN_PUNCTUATOR, "(");
        SyntaxNode *condition = parse_expression(parser);
        if (condition) append_child_syntax_node(if_node, condition);
        parser_match_and_consume(parser, TOKEN_PUNCTUATOR, ")");
        SyntaxNode *then_part = parse_statement(parser);
        if (then_part) append_child_syntax_node(if_node, then_part);
        if (parser_match_and_consume(parser, TOKEN_KEYWORD, "else")) {
            SyntaxNode *else_part = parse_statement(parser);
            if (else_part) append_child_syntax_node(if_node, else_part);
        }
        return if_node;
    }
    
    if (parser_match_and_consume(parser, TOKEN_KEYWORD, "while")) {
        SyntaxNode *while_node = create_syntax_node(NODE_WHILE, current_token->line_number, parser->source_file_length);
        parser_match_and_consume(parser, TOKEN_PUNCTUATOR, "(");
        SyntaxNode *condition = parse_expression(parser);
        if (condition) append_child_syntax_node(while_node, condition);
        parser_match_and_consume(parser, TOKEN_PUNCTUATOR, ")");
        SyntaxNode *body = parse_statement(parser);
        if (body) append_child_syntax_node(while_node, body);
        return while_node;
    }
    
    if (parser_match_and_consume(parser, TOKEN_KEYWORD, "for")) {
        SyntaxNode *for_node = create_syntax_node(NODE_FOR, current_token->line_number, parser->source_file_length);
        parser_match_and_consume(parser, TOKEN_PUNCTUATOR, "(");
        SyntaxNode *init = parse_statement(parser);
        if (init) append_child_syntax_node(for_node, init);
        SyntaxNode *condition = parse_expression(parser);
        if (condition) append_child_syntax_node(for_node, condition);
        parser_match_and_consume(parser, TOKEN_PUNCTUATOR, ";");
        SyntaxNode *step = parse_expression(parser);
        if (step) append_child_syntax_node(for_node, step);
        parser_match_and_consume(parser, TOKEN_PUNCTUATOR, ")");
        SyntaxNode *body = parse_statement(parser);
        if (body) append_child_syntax_node(for_node, body);
        return for_node;
    }
    
    if (parser_match_and_consume(parser, TOKEN_KEYWORD, "return")) {
        SyntaxNode *return_node = create_syntax_node(NODE_RETURN, current_token->line_number, parser->source_file_length);
        if (!parser_check_current_type(parser, TOKEN_PUNCTUATOR, ";")) {
            SyntaxNode *val = parse_expression(parser);
            if (val) append_child_syntax_node(return_node, val);
        }
        parser_match_and_consume(parser, TOKEN_PUNCTUATOR, ";");
        return return_node;
    }
    
    const char *data_types[] = {"int", "float", "char", "double", "long", "short", "void"};
    for (int i = 0; i < 7; i++) {
        if (parser_check_current_type(parser, TOKEN_KEYWORD, data_types[i])) {
            LexicalToken *type_token = parser_advance_token(parser);
            SyntaxNode *decl_node = create_syntax_node(NODE_VAR_DECL, type_token->line_number, parser->source_file_length);
            decl_node->associated_value = strdup(type_token->text_value);
            LexicalToken *name_token = parser_match_and_consume(parser, TOKEN_IDENTIFIER, NULL);
            if (name_token) {
                SyntaxNode *id_node = create_syntax_node(NODE_IDENTIFIER, name_token->line_number, parser->source_file_length);
                id_node->associated_value = strdup(name_token->text_value);
                append_child_syntax_node(decl_node, id_node);
                if (parser_match_and_consume(parser, TOKEN_OPERATOR, "=")) {
                    SyntaxNode *init_expr = parse_expression(parser);
                    if (init_expr) append_child_syntax_node(decl_node, init_expr);
                }
            }
            parser_match_and_consume(parser, TOKEN_PUNCTUATOR, ";");
            return decl_node;
        }
    }
    
    SyntaxNode *expr_node = parse_expression(parser);
    if (expr_node) {
        parser_match_and_consume(parser, TOKEN_PUNCTUATOR, ";");
        SyntaxNode *stmt_wrapper = create_syntax_node(NODE_EXPR_STMT, expr_node->source_line, parser->source_file_length);
        append_child_syntax_node(stmt_wrapper, expr_node);
        return stmt_wrapper;
    }
    return NULL;
}

SyntaxNode *parse_syntax_tree(SyntaxParser *parser) {
    SyntaxNode *program_root = create_syntax_node(NODE_PROGRAM, 1, parser->source_file_length);
    while (parser_lookahead(parser) && parser_lookahead(parser)->category != TOKEN_EOF) {
        LexicalToken *token = parser_lookahead(parser);
        if (token->category == TOKEN_KEYWORD || token->category == TOKEN_IDENTIFIER) {
            int saved_pos = parser->current_token_index;
            parser_advance_token(parser);
            LexicalToken *next = parser_lookahead(parser);
            if (next && next->category == TOKEN_IDENTIFIER) {
                parser_advance_token(parser);
                if (parser_check_current_type(parser, TOKEN_PUNCTUATOR, "(")) {
                    parser->current_token_index = saved_pos;
                    LexicalToken *ret_type = parser_advance_token(parser);
                    LexicalToken *func_name = parser_advance_token(parser);
                    SyntaxNode *function_node = create_syntax_node(NODE_FUNCTION, ret_type->line_number, parser->source_file_length);
                    function_node->associated_value = strdup(func_name->text_value);
                    parser_match_and_consume(parser, TOKEN_PUNCTUATOR, "(");
                    while (!parser_check_current_type(parser, TOKEN_PUNCTUATOR, ")") && parser_lookahead(parser))
                        parser_advance_token(parser);
                    parser_match_and_consume(parser, TOKEN_PUNCTUATOR, ")");
                    SyntaxNode *body = parse_block_statement(parser);
                    if (body) {
                        append_child_syntax_node(function_node, body);
                        append_child_syntax_node(program_root, function_node);
                    }
                    continue;
                }
            }
            parser->current_token_index = saved_pos;
        }
        SyntaxNode *generic_stmt = parse_statement(parser);
        if (generic_stmt) append_child_syntax_node(program_root, generic_stmt);
        else if (parser_lookahead(parser)) parser_advance_token(parser);
    }
    return program_root;
}







static ControlFlowBlock *create_control_flow_block(int id, int file_length) {
    ControlFlowBlock *block = calloc(1, sizeof(ControlFlowBlock));
    block->block_id = id;
    block->statement_nodes = calloc(file_length + 1, sizeof(SyntaxNode *));
    return block;
}

static void link_control_flow_blocks(ControlFlowBlock *source, ControlFlowBlock *destination) {
    if (!source || !destination || source->successor_count >= 2) return;
    for (int i = 0; i < source->successor_count; i++)
        if (source->successor_blocks[i] == destination) return;
    source->successor_blocks[source->successor_count++] = destination;
}

static void traverse_ast_to_build_cfg(ControlFlowGraph *graph, SyntaxNode *node, ControlFlowBlock **current_block) {
    if (!node) return;
    switch (node->node_type) {
        case NODE_BLOCK:
            for (int i = 0; i < node->child_node_count; i++)
                traverse_ast_to_build_cfg(graph, node->child_nodes[i], current_block);
            break;
        case NODE_IF: {
            ControlFlowBlock *then_block = create_control_flow_block(graph->total_blocks, graph->source_file_length);
            graph->all_blocks[graph->total_blocks++] = then_block;
            ControlFlowBlock *else_block = node->child_node_count > 2 ? create_control_flow_block(graph->total_blocks, graph->source_file_length) : NULL;
            if (else_block) graph->all_blocks[graph->total_blocks++] = else_block;
            ControlFlowBlock *merge_block = create_control_flow_block(graph->total_blocks, graph->source_file_length);
            graph->all_blocks[graph->total_blocks++] = merge_block;
            
            link_control_flow_blocks(*current_block, then_block);
            link_control_flow_blocks(*current_block, else_block ? else_block : merge_block);
            
            ControlFlowBlock *temp_block = then_block;
            traverse_ast_to_build_cfg(graph, node->child_nodes[1], &temp_block);
            link_control_flow_blocks(temp_block, merge_block);
            
            if (else_block) {
                temp_block = else_block;
                traverse_ast_to_build_cfg(graph, node->child_nodes[2], &temp_block);
                link_control_flow_blocks(temp_block, merge_block);
            }
            *current_block = merge_block;
            break;
        }
        case NODE_WHILE: {
            ControlFlowBlock *header = create_control_flow_block(graph->total_blocks, graph->source_file_length);
            graph->all_blocks[graph->total_blocks++] = header;
            ControlFlowBlock *body = create_control_flow_block(graph->total_blocks, graph->source_file_length);
            graph->all_blocks[graph->total_blocks++] = body;
            ControlFlowBlock *exit_block = create_control_flow_block(graph->total_blocks, graph->source_file_length);
            graph->all_blocks[graph->total_blocks++] = exit_block;
            
            link_control_flow_blocks(*current_block, header);
            link_control_flow_blocks(header, body);
            link_control_flow_blocks(header, exit_block);
            
            ControlFlowBlock *temp_block = body;
            traverse_ast_to_build_cfg(graph, node->child_nodes[1], &temp_block);
            link_control_flow_blocks(temp_block, header);
            *current_block = exit_block;
            break;
        }
        case NODE_FOR: {
            ControlFlowBlock *init = create_control_flow_block(graph->total_blocks, graph->source_file_length);
            graph->all_blocks[graph->total_blocks++] = init;
            ControlFlowBlock *header = create_control_flow_block(graph->total_blocks, graph->source_file_length);
            graph->all_blocks[graph->total_blocks++] = header;
            ControlFlowBlock *body = create_control_flow_block(graph->total_blocks, graph->source_file_length);
            graph->all_blocks[graph->total_blocks++] = body;
            ControlFlowBlock *exit_block = create_control_flow_block(graph->total_blocks, graph->source_file_length);
            graph->all_blocks[graph->total_blocks++] = exit_block;
            
            link_control_flow_blocks(*current_block, init);
            if (node->child_node_count > 0) init->statement_nodes[init->statement_count++] = node->child_nodes[0];
            link_control_flow_blocks(init, header);
            link_control_flow_blocks(header, body);
            link_control_flow_blocks(header, exit_block);
            
            ControlFlowBlock *temp_block = body;
            if (node->child_node_count > 3) traverse_ast_to_build_cfg(graph, node->child_nodes[3], &temp_block);
            if (node->child_node_count > 2) temp_block->statement_nodes[temp_block->statement_count++] = node->child_nodes[2];
            link_control_flow_blocks(temp_block, header);
            *current_block = exit_block;
            break;
        }
        default:
            (*current_block)->statement_nodes[(*current_block)->statement_count++] = node;
            break;
    }
}

ControlFlowGraph *construct_control_flow_graph(SyntaxNode *function_node, int file_length) {
    if (!function_node || function_node->node_type != NODE_FUNCTION) return NULL;
    ControlFlowGraph *graph = calloc(1, sizeof(ControlFlowGraph));
    graph->source_file_length = file_length;
    graph->all_blocks = calloc(file_length + 1, sizeof(ControlFlowBlock *));
    ControlFlowBlock *entry = create_control_flow_block(0, file_length);
    graph->all_blocks[graph->total_blocks++] = entry;
    ControlFlowBlock *current = entry;
    if (function_node->child_node_count > 0) traverse_ast_to_build_cfg(graph, function_node->child_nodes[0], &current);
    return graph;
}

void release_control_flow_graph(ControlFlowGraph *graph) {
    if (!graph) return;
    for (int i = 0; i < graph->total_blocks; i++) {
        ControlFlowBlock *block = graph->all_blocks[i];
        free(block->statement_nodes);
        free(block);
    }
    free(graph->all_blocks);
    free(graph);
}






static void generate_token_sequence_hash(LexicalToken **tokens, int start_index, int window_size, CodeFingerprint *output_fingerprint) {
    unsigned int cumulative_hash = 0;
    int min_line = 1000000, max_line = 0;
    for (int i = 0; i < window_size; i++) {
        LexicalToken *token = tokens[start_index + i];
        if (token->line_number < min_line) min_line = token->line_number;
        if (token->line_number > max_line) max_line = token->line_number;
        cumulative_hash = cumulative_hash * 31 + token->category;
        if (token->category == TOKEN_KEYWORD || token->category == TOKEN_OPERATOR)
            for (char *p = token->text_value; *p; p++) cumulative_hash = cumulative_hash * 31 + *p;
    }
    output_fingerprint->hash_value = cumulative_hash;
    output_fingerprint->start_line = min_line;
    output_fingerprint->end_line = max_line;
}

static int perform_winnowing(LexicalToken **tokens, int total_tokens, int k_gram_size, int window_size, CodeFingerprint *fingerprint_buffer) {
    int total_fingerprints = total_tokens - k_gram_size + 1;
    if (total_fingerprints <= 0) return 0;
    CodeFingerprint *all_gram_hashes = malloc(sizeof(CodeFingerprint) * total_fingerprints);
    for (int i = 0; i < total_fingerprints; i++) generate_token_sequence_hash(tokens, i, k_gram_size, &all_gram_hashes[i]);
    
    unsigned int last_recorded_hash = 0;
    int selected_fingerprint_count = 0;
    for (int i = 0; i < total_fingerprints - window_size + 1; i++) {
        int min_index_in_window = i;
        for (int j = 1; j < window_size; j++)
            if (all_gram_hashes[i + j].hash_value <= all_gram_hashes[min_index_in_window].hash_value)
                min_index_in_window = i + j;
        if (i == 0 || all_gram_hashes[min_index_in_window].hash_value != last_recorded_hash) {
            fingerprint_buffer[selected_fingerprint_count++] = all_gram_hashes[min_index_in_window];
            last_recorded_hash = all_gram_hashes[min_index_in_window].hash_value;
        }
    }
    free(all_gram_hashes);
    return selected_fingerprint_count;
}

static double calculate_token_similarity(LexicalToken **tokens1, int count1, LexicalToken **tokens2, int count2, MatchedCodeRegion *regions, int *region_count) {
    if (!count1 || !count2) return 0;
    CodeFingerprint *fps1 = malloc(sizeof(CodeFingerprint) * (count1 + 1));
    CodeFingerprint *fps2 = malloc(sizeof(CodeFingerprint) * (count2 + 1));
    int win_count1 = perform_winnowing(tokens1, count1, 5, 4, fps1);
    int win_count2 = perform_winnowing(tokens2, count2, 5, 4, fps2);
    
    if (!win_count1 || !win_count2) {
        free(fps1); free(fps2);
        return count1 == count2;
    }
    
    int matching_hashes = 0;
    for (int i = 0; i < win_count1; i++) {
        for (int j = 0; j < win_count2; j++) {
            if (fps1[i].hash_value == fps2[j].hash_value) {
                matching_hashes++;
                if (regions && region_count) {
                    regions[*region_count].start_line_file1 = fps1[i].start_line;
                    regions[*region_count].end_line_file1 = fps1[i].end_line;
                    regions[*region_count].start_line_file2 = fps2[j].start_line;
                    regions[*region_count].end_line_file2 = fps2[j].end_line;
                    (*region_count)++;
                }
                break;
            }
        }
    }
    double jaccard_similarity = (double)matching_hashes / (win_count1 + win_count2 - matching_hashes);
    free(fps1); free(fps2);
    return jaccard_similarity;
}

static double recursively_compare_syntax_node_structures(SyntaxNode *node1, SyntaxNode *node2) {
    if (!node1 || !node2) return node1 == node2;
    if (node1->node_type != node2->node_type) return 0;
    int max_children = node1->child_node_count > node2->child_node_count ? node1->child_node_count : node2->child_node_count;
    if (!max_children) return 1;
    double cumulative_similarity = 0;
    int min_children = node1->child_node_count < node2->child_node_count ? node1->child_node_count : node2->child_node_count;
    for (int i = 0; i < min_children; i++)
        cumulative_similarity += recursively_compare_syntax_node_structures(node1->child_nodes[i], node2->child_nodes[i]);
    return (1.0 + cumulative_similarity) / (1.0 + max_children);
}

static double calculate_control_flow_similarity(SyntaxNode *ast1, SyntaxNode *ast2, int len1, int len2) {
    SyntaxNode *func1 = NULL, *func2 = NULL;
    for (int i = 0; i < ast1->child_node_count; i++) if (ast1->child_nodes[i]->node_type == NODE_FUNCTION) { func1 = ast1->child_nodes[i]; break; }
    for (int i = 0; i < ast2->child_node_count; i++) if (ast2->child_nodes[i]->node_type == NODE_FUNCTION) { func2 = ast2->child_nodes[i]; break; }
    if (!func1 || !func2) return 0;
    ControlFlowGraph *cfg1 = construct_control_flow_graph(func1, len1);
    ControlFlowGraph *cfg2 = construct_control_flow_graph(func2, len2);
    double similarity = 0;
    if (cfg1 && cfg2) {
        int count1 = cfg1->total_blocks, count2 = cfg2->total_blocks;
        similarity = (double)(count1 < count2 ? count1 : count2) / (count1 > count2 ? count1 : count2);
    }
    release_control_flow_graph(cfg1); release_control_flow_graph(cfg2);
    return similarity;
}

SimilarityAnalysisResult analyze_file_similarity(const char *path1, const char *path2) {
    char *content1 = load_file_content_to_string(path1);
    char *content2 = load_file_content_to_string(path2);
    SimilarityAnalysisResult result = {0, 0, 0, 0, NULL, 0};
    if (!content1 || !content2) { free(content1); free(content2); return result; }
    
    int length1 = (int)strlen(content1), length2 = (int)strlen(content2);
    LexicalToken **tokens1 = calloc(length1 + 1, sizeof(LexicalToken *));
    LexicalToken **tokens2 = calloc(length2 + 1, sizeof(LexicalToken *));
    
    LexicalScanner *scanner1 = initialize_lexical_scanner(content1);
    LexicalScanner *scanner2 = initialize_lexical_scanner(content2);
    int count1 = tokenize_source_code(scanner1, tokens1);
    int count2 = tokenize_source_code(scanner2, tokens2);
    
    SyntaxParser *parser1 = initialize_syntax_parser(tokens1, count1, length1);
    SyntaxParser *parser2 = initialize_syntax_parser(tokens2, count2, length2);
    SyntaxNode *ast1 = parse_syntax_tree(parser1);
    SyntaxNode *ast2 = parse_syntax_tree(parser2);
    
    result.matched_regions = calloc(length1 + length2 + 1, sizeof(MatchedCodeRegion));
    result.token_similarity_score = calculate_token_similarity(tokens1, count1, tokens2, count2, result.matched_regions, &result.total_matches_found);
    result.structural_ast_score = recursively_compare_syntax_node_structures(ast1, ast2);
    result.control_flow_cfg_score = calculate_control_flow_similarity(ast1, ast2, length1, length2);
    result.overall_final_score = result.token_similarity_score * 0.3 + result.structural_ast_score * 0.4 + result.control_flow_cfg_score * 0.3;
    
    recursively_release_ast(ast1); recursively_release_ast(ast2);
    free(parser1); free(parser2);
    for (int i = 0; i < count1; i++) release_lexical_token(tokens1[i]);
    for (int i = 0; i < count2; i++) release_lexical_token(tokens2[i]);
    free(tokens1); free(tokens2);
    free(scanner1); free(scanner2);
    free(content1); free(content2);
    return result;
}






void display_comparison_report(const char *file1, const char *file2, SimilarityAnalysisResult result) {
    printf("Comparing %s and %s:\n", file1, file2);
    printf("  Lexical Token Score:  %.2f%%\n", result.token_similarity_score * 100);
    printf("  Structural AST Score: %.2f%%\n", result.structural_ast_score * 100);
    printf("  Control Flow CFG Score: %.2f%%\n", result.control_flow_cfg_score * 100);
    printf("  Overall Final Similarity: %.2f%%\n", result.overall_final_score * 100);
    
    if (result.matched_regions && result.total_matches_found > 0) {
        printf("  Identified Matching Regions:\n");
        int last_displayed_start = -1;
        for (int i = 0; i < result.total_matches_found; i++) {
            MatchedCodeRegion region = result.matched_regions[i];
            if (region.start_line_file1 != last_displayed_start) {
                printf("    File 1 lines %d-%d matched File 2 lines %d-%d\n", 
                       region.start_line_file1, region.end_line_file1, 
                       region.start_line_file2, region.end_line_file2);
                last_displayed_start = region.start_line_file1;
            }
        }
    }
    
    const char *confidence_level = result.overall_final_score > 0.8 ? "HIGH" : (result.overall_final_score > 0.5 ? "SUSPICIOUS" : "LOW");
    printf("  Analysis Confidence: %s\n--------------------------------------\n", confidence_level);
    if (result.matched_regions) free(result.matched_regions);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Error: Missing arguments.\nUsage: %s file1.c file2.c\n", argv[0]);
        return 1;
    }
    display_comparison_report(argv[1], argv[2], analyze_file_similarity(argv[1], argv[2]));
    return 0;
}
