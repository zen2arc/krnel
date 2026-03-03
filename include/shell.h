#ifndef SHELL_H
#define SHELL_H

void shell_init(void);
void shell_run(void);
void execute_command(char* line);
void shell_prompt_redraw(void);
int shell_logged_in(void);

#endif
