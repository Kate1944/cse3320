// The MIT License (MIT)
//
// Copyright (c) 2024 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#define WHITESPACE " \t\n" // We want to split our command line up into tokens
                           // so we need to define what delimits our tokens.
                           // In this case  white space
                           // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255 // The maximum command-line size

#define MAX_NUM_ARGUMENTS 32

int main(int argc, char *argv[])
{
  char *command_string = (char *)malloc(MAX_COMMAND_SIZE);
  // shell continues after error message unless bad batch file
  char error_message[30] = "An error has occurred\n";

  if (argc > 2)
  {
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(1);
  }

  if (argv[1] != NULL)
  {
    FILE *fp;
    char input_str[100];

    fp = fopen(argv[1], "r");

    if (fp == NULL)
    {
      write(STDERR_FILENO, error_message, strlen(error_message));
      exit(1);
    }
    else
    {
      char *argument;
      char *line;
      bool quit = false;
      bool cd = false;
      bool check_one_arg = false;
      while ((line = fgets(input_str, 100, fp)) != NULL)
      {
        while ((argument = strsep(&line, " \n")) != NULL)
        {
          if (check_one_arg == true)
          {
            // if more than one argument given after cd, error (this arg should be end line)
            if ((strcmp(argument, "")) != 0)
            {
              write(STDERR_FILENO, error_message, strlen(error_message));
              exit(0);
            }
            check_one_arg = false;
          }
          if (quit == true)
          {
            // if arguments given after exit, error
            if ((strcmp(argument, "")) != 0)
            {
              write(STDERR_FILENO, error_message, strlen(error_message));
              exit(0);
            }
            else
            {
              exit(0);
            }
          }
          if (cd == true)
          {
            // if no argument given after cd, error
            if ((strcmp(argument, "")) == 0)
            {
              write(STDERR_FILENO, error_message, strlen(error_message));
              exit(0);
            }
            else
            {
              chdir(argument);
              check_one_arg = true;
              // ensure chdir performed successfully
              if (chdir(argument) != 0)
              {
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(0);
              }
            }
            cd = false;
          }
          // exit and quit
          if ((strcmp(argument, "exit")) == 0 || (strcmp(argument, "quit")) == 0)
          {
            quit = true;
          }
          // cd
          if ((strcmp(argument, "cd")) == 0)
          {
            cd = true;
          }
        }
      }
    }
    fclose(fp);
  }
  else
  {
    while (1)
    {
      // Print out the msh prompt
      printf("msh> ");

      // Read the command from the commandi line.  The
      // maximum command that will be read is MAX_COMMAND_SIZE
      // This while command will wait here until the user
      // inputs something.
      while (!fgets(command_string, MAX_COMMAND_SIZE, stdin))
        ;

      /* Parse input */
      char *token[MAX_NUM_ARGUMENTS];

      int token_count = 0;

      // Pointer to point to the token
      // parsed by strsep
      char *argument_pointer;

      char *working_string = strdup(command_string);

      // we are going to move the working_string pointer so
      // keep track of its original value so we can deallocate
      // the correct amount at the end

      char *head_ptr = working_string;

      // Tokenize the input with whitespace used as the delimiter
      while (((argument_pointer = strsep(&working_string, WHITESPACE)) != NULL) &&
             (token_count < MAX_NUM_ARGUMENTS))
      {
        token[token_count] = strndup(argument_pointer, MAX_COMMAND_SIZE);
        if (strlen(token[token_count]) == 0)
        {
          token[token_count] = NULL;
        }
        token_count++;
      }

      // Now print the tokenized input as a debug check
      // \TODO Remove this code and replace with your shell functionality

      int token_index = 0;
      for (token_index = 0; token_index < token_count; token_index++)
      {
        printf("token[%d] = %s\n", token_index, token[token_index]);
      }

      if (token[0] != NULL)
      {
        //----------------built in commands---------------------
        // exit and quit
        if ((strcmp(token[0], "exit")) == 0 || (strcmp(token[0], "quit")) == 0)
        {
          // ensure no arguments are passed after exit command
          for (int i = 1; i < token_count; i++)
          {
            if (token[i] != NULL)
            {
              write(STDERR_FILENO, error_message, strlen(error_message));
            }
          }
          exit(0);
        }

        // cd
        else if ((strcmp(token[0], "cd")) == 0)
        {
          // ensure it only takes exactly one consecutive argument (ending NULL token makes token_count=3)
          if (token_count != 3)
          {
            write(STDERR_FILENO, error_message, strlen(error_message));
          }
          if (token[1] != NULL)
          {
            chdir(token[1]);
            if (chdir(token[1]) != 0)
            {
              write(STDERR_FILENO, error_message, strlen(error_message));
            }
          }
        }
        //----------------------all other commands--------------------------
        else
        {
          pid_t child_pid = fork();
          int status;
          bool exec = false;
          char path1[50] = "/bin/";
          char path2[50] = "/usr/bin/";
          char path3[50] = "/usr/local/bin/";
          char path4[50] = "./";
          char found_path[50];

          if (child_pid == 0)
          {

            // check if command is executable in each path
            // first, append command to each filepath
            strcat(path1, token[0]);
            strcat(path2, token[0]);
            strcat(path3, token[0]);
            strcat(path4, token[0]);
            
            if ((access(path1, X_OK)) != -1)
            {
              exec = true;
              for (int i = 0; i < sizeof(path1); i++)
              {
                found_path[i] = path1[i];
              }
            }
            else if ((access(path2, X_OK)) != -1)
            {
              exec = true;
              for (int i = 0; i < sizeof(path2); i++)
              {
                found_path[i] = path2[i];
              }
            }
            else if ((access(path3, X_OK)) != -1)
            {
              exec = true;
              for (int i = 0; i < sizeof(path3); i++)
              {
                found_path[i] = path3[i];
              }
            }
            else if ((access(path4, X_OK)) != -1)
            {
              exec = true;
              for (int i = 0; i < sizeof(path4); i++)
              {
                found_path[i] = path4[i];
              }
            }
            if (exec == true)
            {
              execv(found_path, token);
              write(STDERR_FILENO, strerror(errno), strlen(strerror(errno)));
              exit(0);
            }
            else
            {
              write(STDERR_FILENO, error_message, strlen(error_message));
            }
          }
          else if (child_pid == -1)
          {
            write(STDERR_FILENO, error_message, strlen(error_message));
          }
          else
          {
            waitpid(child_pid, &status, 0);
          }
        }
      }

      free(head_ptr);
    }
  }

  return 0;
}
