#include <iostream>     
#include <sstream>      // stringstream for tokenizing input
#include <vector>       
#include <string>       
#include <unistd.h>     // fork, execvp, chdir, getcwd, dup2
#include <sys/wait.h>   // wait()
#include <dirent.h>     // DIR, opendir, readdir
#include <limits.h>     // PATH_MAX
#include <fcntl.h>      // open() for redirection
#include <cstdlib>      // exit(), setenv(), getenv()
#include <fstream>      // ifstream for batch mode 

using namespace std;

extern char **environ; // access environment variables

// Helper Functions

// Split input into tokens
vector<string> tokenize(string input) {
    vector<string> tokens;
    stringstream ss(input);
    string word; 
    while (ss >> word) 
        tokens.push_back(word);
    return tokens; 
}

// Display prompt with current directory
void displayPrompt() {
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    cout << cwd << " > "; 
}

// List directory contents
void listDirectory(string path) {
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        perror("dir"); 
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
        cout << entry->d_name << endl;
    closedir(dir);
}

// Print all environment variables
void printEnvironment() {
    for (char **env = environ; *env != NULL; env++)
        cout << *env << endl;
}

// Pause shell until Enter is pressed
void pauseShell() {
    cout << "Shell paused. Press Enter to continue...";
    cin.ignore();
    cin.get();
}

// Simple help menu
void help() {
    cout << "\n----- MyShell Help -----\n";
    cout << "cd [dir]        Change directory\n";
    cout << "dir [dir]       List directory contents\n";
    cout << "environ         List environment variables\n";
    cout << "set var value   Set environment variable\n";
    cout << "echo text       Display text\n";
    cout << "pause           Wait for Enter\n";
    cout << "quit            Exit shell\n";
    cout << "Supports redirection (<, >, >>) and background (&)\n";
}

//Execute External Commands

void executeExternal(vector<string> tokens, bool background,
                     string inputFile, string outputFile, bool append) {

    vector<char*> args;
    for (auto &t : tokens)
        args.push_back(&t[0]);
    args.push_back(NULL);

    pid_t pid = fork();

    if (pid < 0) { 
        perror("fork failed");
        return;
    }

    if (pid == 0) { // Child process

        // Input redirection
        if (!inputFile.empty()) {
            int fd = open(inputFile.c_str(), O_RDONLY);
            if (fd < 0) { perror("input redirection"); exit(1); }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // Output redirection
        if (!outputFile.empty()) {
            int fd;
            if (append)
                fd = open(outputFile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            else
                fd = open(outputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (fd < 0) { perror("output redirection"); exit(1); }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(args[0], args.data()); 
        perror("exec failed"); // Only runs if execvp fails
        exit(1); 
    }
    else { // Parent process
        if (!background)
            wait(NULL);
        else
            cout << "Process running in background PID: " << pid << endl;
    }
}

//Command Handler

void processCommand(string input) {

    bool background = false;
    string inputFile = "";
    string outputFile = "";
    bool append = false;

    vector<string> tokens = tokenize(input);
    if (tokens.empty()) return;

    // Check for background execution &
    if (tokens.back() == "&") {
        background = true;
        tokens.pop_back();
    }

    // Handle I/O redirection
    vector<string> cleaned;
    for (int i = 0; i < tokens.size(); i++) {
        if (tokens[i] == "<") { inputFile = tokens[i+1]; i++; }
        else if (tokens[i] == ">") { outputFile = tokens[i+1]; append = false; i++; }
        else if (tokens[i] == ">>") { outputFile = tokens[i+1]; append = true; i++; }
        else cleaned.push_back(tokens[i]);
    }

    if (cleaned.empty()) return;
    string cmd = cleaned[0];

    //Built-in Commands 

    if (cmd == "cd") {
        char cwd[PATH_MAX];
        if (cleaned.size() == 1) 
            cout << getcwd(cwd, sizeof(cwd)) << endl;
        else {
            if (chdir(cleaned[1].c_str()) != 0) perror("cd");
            getcwd(cwd, sizeof(cwd));
            setenv("PWD", cwd, 1);
        }
    }
    else if (cmd == "dir") { 
        string path = (cleaned.size() > 1) ? cleaned[1] : ".";
        listDirectory(path);
    }
    else if (cmd == "environ") { printEnvironment(); }
    else if (cmd == "set") {
        if (cleaned.size() >= 3) setenv(cleaned[1].c_str(), cleaned[2].c_str(), 1);
        else cout << "Usage: set VARIABLE VALUE\n";
    }
    else if (cmd == "echo") {
        for (int i = 1; i < cleaned.size(); i++) cout << cleaned[i] << " ";
        cout << endl;
    }
    else if (cmd == "help") { help(); } 
    else if (cmd == "pause") { pauseShell(); } 
    else if (cmd == "quit") { exit(0); }
    else {
        executeExternal(cleaned, background, inputFile, outputFile, append);
    }
}

//Main Function

int main(int argc, char *argv[]) {

    // Batch mode
    if (argc > 1) { 
        ifstream file(argv[1]);
        if (!file) { cout << "Batch file not found\n"; return 1; }
        string line; 
        while (getline(file, line)) processCommand(line);
        return 0; 
    }
  
    // Interactive mode
    string input;
        while (true) {
            displayPrompt();
            getline(cin, input); 
            processCommand(input);
        } 

        return 0;
    }  
