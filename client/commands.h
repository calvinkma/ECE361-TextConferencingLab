static char LOGIN_COMMAND_PATTERN[] = "^/login[[:space:]][[:alnum:]|_]+[[:space:]][[:alnum:]|_|[:punct:]]+[[:space:]]+[[:digit:]|.]+[[:space:]]+[[:digit:]]+[[:space:]]*$";
static char LOGOUT_COMMAND_PATTERN[] = "^/logout[[:space:]]*$";
static char JOIN_SESS_COMMAND_PATTERN[] = "^/joinsession[[:space:]][[:alnum:]|_]+[[:space:]]*$";
static char LEAVE_SESS_COMMAND_PATTERN[] = "^/leavesession[[:space:]]*$";
static char LEAVE_SESS_NAME_COMMAND_PATTERN[] = "^/leavesession[[:space:]][[:alnum:]|_]+[[:space:]]*$";
static char CREATE_SESS_COMMAND_PATTERN[] = "^/createsession[[:space:]][[:alnum:]|_]+[[:space:]]*$";
static char LIST_COMMAND_PATTERN[] = "^/list[[:space:]]*$";
static char QUIT_COMMAND_PATTERN[] = "^/quit[[:space:]]*$";

bool is_string_of_pattern(char* input_buf, char* command_regex_pattern) {
    regex_t regex;
    int r = regcomp(&regex, command_regex_pattern, REG_EXTENDED);
    if (r != 0) {
        printf("When checking %s against %s, the regex does not compile.\n", input_buf, command_regex_pattern);
        exit(EXIT_FAILURE);
    }
    return regexec(&regex, input_buf, 0, NULL, 0) == 0;
}