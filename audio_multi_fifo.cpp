/*
   Copyright (c) 2018, Adrian Rossiter

   Antiprism - http://www.antiprism.com

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

      The above copyright notice and this permission notice shall be included
      in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include "programopts.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <locale.h>
#include <errno.h>

#include <math.h>
#include <vector>
#include <string>

using std::vector;
using std::string;

const char *PROGRAM_NAME = "audio_multi_fifo";

class MultiOpts : public ProgramOpts {
public:
  string version;
  string conf_file;

  MultiOpts(): ProgramOpts(PROGRAM_NAME, "0.01"),
      conf_file(string(PROGRAM_NAME)+".conf")
      {}
  void process_command_line(int argc, char **argv);
  void usage();
};

// clang-format off
void MultiOpts::usage()
{
  fprintf(stdout,
"\n"
"Usage: %s [options]\n"
"\n"
"Split audio input on standard input into multiple FIFOs in /tmp. If a FIFO\n"
"cannot be written to, e.g. because it is not being read and its buffer is\n"
"full, the data to be written will be discarded\n"
"\n"
"Options\n"
"%s"
"  -c <file>  configuration file (default: %s)\n"
"\n",
  get_program_name().c_str(), help_ver_text, conf_file.c_str());
}
// clang-format on

void MultiOpts::process_command_line(int argc, char **argv)
{
  opterr = 0;
  int c;

  handle_long_opts(argc, argv);

  while ((c=getopt(argc, argv, ":hc:")) != -1) {
    if (common_opts(c, optopt))
      continue;

    switch (c) {
    case 'c':
      conf_file = optarg;
      break;

    default:
      error("unknown command line error");
    }
  }
}


int start_idle_loop(vector<FILE *> fifos)
{
  const size_t buff_sz = 1024;
  uint16_t buff[buff_sz];
  while (true) {
    // If there is data read it
    size_t num_read = fread(buff, sizeof(uint16_t), buff_sz, stdin);
    for(size_t i=0; i<fifos.size(); i++)
      fwrite(buff, sizeof(uint16_t), num_read, fifos[i]);
  }

  return 0;
}


int main(int argc, char **argv)
{
  MultiOpts opts;
  opts.process_command_line(argc, argv);

  vector<string> fifo_names;
  fifo_names.push_back("test2");
  fifo_names.push_back("test3");

  vector<FILE *> fifos;
  for(size_t i=0; i<fifo_names.size(); i++) {
    const char *illegal_chars = "\\/:?\"<>|";
    if(strpbrk(fifo_names[i].c_str(), illegal_chars))
      opts.error("FIFO name " + fifo_names[i] +
                 "contains an illegal character from: " + illegal_chars);
    const string fifo_name = "/tmp/"+fifo_names[i];
    unlink(fifo_name.c_str());
    if(mkfifo(fifo_name.c_str(), 0666) == -1)
      opts.error("could not create FIFO " + fifo_names[i] +
                 " : " + string(strerror(errno)));
    FILE *ofile = fopen(fifo_name.c_str(), "w");
    if(ofile == NULL)
      opts.error("could not open FIFO " + fifo_name +
                 " for writing: " + string(strerror(errno)));

    int fd = fileno(ofile);
    int f = fcntl(fd, F_GETFL, 0);
    f |= O_NONBLOCK;
    fcntl(fd, F_SETFL, f);

    fifos.push_back(ofile);
  }

  int loop_ret = start_idle_loop(fifos);

  if(loop_ret != 0)
    exit(EXIT_FAILURE);

  return 0;
}
