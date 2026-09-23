void executeCommand(char *command, char **args, int input_fd, int output_fd, int *status, int is_bg);
int parseInput(char *buffer, char **tokens);
