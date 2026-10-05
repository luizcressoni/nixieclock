/*! \file parse_stdin.h */
#pragma once

char *parse_stdin();
char *get_form_name();
char *get_field_value(const char *_fieldname);
void free_buffer();
//eof parse_stdin.h