#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
ReadEntireFile(const char *filepath)
{
    char *result = 0;
    
    FILE *fp = fopen(filepath, "rb");
    if (fp)
    {
        fseek(fp, 0, SEEK_END);
        size_t file_size = ftell(fp);
        rewind(fp);
        
        result = (char *)malloc(file_size + 1);
        fread(result, file_size, 1, fp);
        
        // NOTE(Sixten): Null-terminate.
        result[file_size] = 0;
        
        fclose(fp);
    }
    
    return result;
}

static void
PrintHelpMessage()
{
    printf("Usage: cml [input_file] (optional [output_file])\n");
}

enum TokenType
{
    Token_Unkown,
    Token_Identifier,
    Token_String,
    Token_At,
    Token_Equals,
    Token_OpenParen,
    Token_CloseParen,
    Token_OpenBraces,
    Token_CloseBraces,
    Token_EndOfFile
};

struct Tokenizer
{
    char *at;
};

struct Token
{
    TokenType type;
    
    int text_length;
    char *text;
};

static bool
IsWhitespace(char c)
{
    bool result = ((c == ' ') ||
                   (c == '\n') ||
                   (c == '\r') ||
                   (c == '\t'));
    
    return result;
}

static bool
IsAlpha(char c)
{
    return ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z'));
}

static bool
IsNumber(char c)
{
    return (c >= '0' && c <= '9');
}

static void
RemoveWhitespace(Tokenizer *tokenizer)
{
    while (IsWhitespace(tokenizer->at[0])) ++tokenizer->at;
}

static Token
GetToken(Tokenizer *tokenizer)
{
    RemoveWhitespace(tokenizer);
    
    Token token = {};
    token.text_length = 1;
    token.text = tokenizer->at;
    
    switch (tokenizer->at[0])
    {
        case '@': { ++tokenizer->at; token.type = Token_At; } break;
        case '=': { ++tokenizer->at; token.type = Token_Equals; } break;
        case '(': { ++tokenizer->at; token.type = Token_OpenParen; } break;
        case ')': { ++tokenizer->at; token.type = Token_CloseParen; } break;
        case '{': { ++tokenizer->at; token.type = Token_OpenBraces; } break;
        case '}': { ++tokenizer->at; token.type = Token_CloseBraces; } break;
        case '\0': { ++tokenizer->at; token.type = Token_EndOfFile; } break;
        
        case '"':
        {
            token.type = Token_String;
            
            ++tokenizer->at;
            token.text = tokenizer->at;
            
            while (tokenizer->at[0] && tokenizer->at[0] != '"')
            {
                ++tokenizer->at;
            }
            
            token.text_length = tokenizer->at - token.text;
            
            if (tokenizer->at[0] == '"')
            {
                ++tokenizer->at;
            }
        } break;
        
        default:
        {
            if (IsAlpha(tokenizer->at[0]))
            {
                token.type = Token_Identifier;
                while (IsAlpha(tokenizer->at[0]) ||
                       IsNumber(tokenizer->at[0]) ||
                       tokenizer->at[0] == '_' ||
                       tokenizer->at[0] == '-')
                {
                    ++tokenizer->at;
                }
                
                token.text_length = tokenizer->at - token.text;
            }
        } break;
    }
    
    return token;
}

#define MAX_CHILDREN 128
#define MAX_NAME_LENGTH 64
#define MAX_CONTENT_LENGTH 1024
#define MAX_DATA 64
#define MAX_DATA_KEY_SIZE 64
#define MAX_DATA_VALUE_SIZE 128

struct Element
{
    Element *children[MAX_CHILDREN];
    int child_count;
    
    char name[MAX_NAME_LENGTH];
    
    char content[MAX_CONTENT_LENGTH];
    
    struct
    {
        char key[MAX_DATA_KEY_SIZE];
        char value[MAX_DATA_VALUE_SIZE];
    } data[MAX_DATA];
    
    int data_count;
};

static bool
RequireToken(Tokenizer *tokenizer, TokenType type)
{
    Token token = GetToken(tokenizer);
    bool result = token.type == type;
    
    return result;
}

static Token
CheckToken(Tokenizer tokenizer)
{
    Token result = GetToken(&tokenizer);
    
    return result;
}

static Token
ParseElement(Tokenizer *tokenizer, Element *element, const Token *element_token)
{
    strncpy(element->name, element_token->text, element_token->text_length);
    
    Token token = GetToken(tokenizer);
    
    if (token.type == Token_OpenParen)
    {
        token = GetToken(tokenizer);
        
        while (token.type != Token_CloseParen)
        {
            if (token.type == Token_Identifier)
            {
                strncpy(element->data[element->data_count].key,
                        token.text, token.text_length);
                
                if (RequireToken(tokenizer, Token_Equals))
                {
                    token = GetToken(tokenizer);
                    
                    if (token.type == Token_String)
                    {
                        strncpy(element->data[element->data_count].value,
                                token.text, token.text_length);
                        
                        ++element->data_count;
                    }
                }
                else
                {
                    printf("Syntax error. Invalid token after value.\n");
                    break;
                }
            }
            else
            {
                printf("Syntax error. Element data value needs to be a string.\n");
                break;
            }
            
            token = GetToken(tokenizer);
        }
        
        token = GetToken(tokenizer);
    }
    
    if (token.type == Token_String)
    {
        strncpy(element->content, token.text, token.text_length);
        
        token = GetToken(tokenizer);
    }
    
    if (token.type == Token_OpenBraces)
    {
        token = GetToken(tokenizer);
        
        while (token.type != Token_CloseBraces)
        {
            Element *child = (Element *)malloc(sizeof(Element));
            
            element->children[element->child_count++] = child;
            
            token = ParseElement(tokenizer, child, &token);
        }
        
        token = GetToken(tokenizer);
    }
    
    return token;
}

static void
OutputElementToHandle(Element *element, FILE *fp)
{
    fprintf(fp, "<%s", element->name);
    for (int i = 0; i < element->data_count; i++)
    {
        fprintf(fp, " %s=\"%s\"", element->data[i].key, element->data[i].value);
    }
    
    if (element->child_count  == 0 && strlen(element->content) == 0)
    {
        fprintf(fp, "/>");
    }
    else
    {
        fprintf(fp, ">%s", element->content);
        
        for (int i = 0; i < element->child_count; i++)
        {
            OutputElementToHandle(element->children[i], fp);
        }
        
        fprintf(fp, "</%s>", element->name);
    }
}

static void
OutputElementToFile(Element *element, const char *file_path)
{
    FILE *fp = fopen(file_path, "wb");
    
    if (fp)
    {
        OutputElementToHandle(element, fp);
        fclose(fp);
    }
}

static void
FreeElement(Element *element, bool free_self)
{
    for (int i = 0; i < element->child_count; ++i)
    {
        FreeElement(element->children[i], true);
    }
    
    if (free_self)
    {
        free(element);
    }
}

static void
ReplaceExtension(const char *source, char *dest, const char *extension)
{
    int last_dot_index;
    
    for (int i = 0; i < strlen(source); ++i)
    {
        if (source[i] == '.')
        {
            last_dot_index = i;
        }
    }
    
    strncpy(dest, source, last_dot_index + 1);
    strcat(dest, extension);
}

int
main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("cml: Invalid usage.\n");
        PrintHelpMessage();
        
        return -1;
    }
    
    if (strcmp(argv[1], "help") == 0)
    {
        PrintHelpMessage();
        
        return 0;
    }
    
    char *input_file_path = argv[1];
    char output_file_path[128];
    
    if (argc == 3)
    {
        strcpy(output_file_path, argv[2]);
    }
    else
    {
        ReplaceExtension(input_file_path, output_file_path, "html");
    }
    
    char *input = ReadEntireFile(input_file_path);
    
    Tokenizer tokenizer = {};
    tokenizer.at = input;
    
    bool parsing = true;
    
    Element document = {};
    
    // NOTE(Sixten): Parse file
    {
        Token token = GetToken(&tokenizer);
        
        ParseElement(&tokenizer, &document, &token);
    }
    
    OutputElementToFile(&document, output_file_path);
    
    FreeElement(&document, false);
    
    free(input);
}