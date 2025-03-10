#include <assert.h>
#include <bits/getopt_core.h>
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <linux/limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// Max count of files that can be proccesed
#define MAX_FILES 1024

static struct ls_options {
  bool show_hidden;
  bool long_format;

  bool debug;
} lsopts;

static struct allignment {
  size_t max_link;
  size_t max_user;
  size_t max_group;
  size_t max_size;
} allign;

int create_path_string(char *str, size_t size, const char *directory,
                       const char *filename) {
  char last_symb = directory[strlen(directory) - 1];
  if (last_symb == '.') {
    snprintf(str, size, "%s", filename);
  } else if (last_symb == '/') {
    snprintf(str, size, "%s%s", directory, filename);
  } else {
    snprintf(str, size, "%s/%s", directory, filename);
  }
}

void print_info(const char *file) {
  if (lsopts.long_format) {
    struct stat file_stat;
    if (stat(file, &file_stat) == -1) {
      perror(strerror(errno));
      exit(EXIT_FAILURE);
    }

    printf((S_ISDIR(file_stat.st_mode)) ? "d" : "-");
    printf((S_IRUSR & file_stat.st_mode) ? "r" : "-");
    printf((S_IWUSR & file_stat.st_mode) ? "w" : "-");
    printf((S_IXUSR & file_stat.st_mode) ? "x" : "-");
    printf((S_IRGRP & file_stat.st_mode) ? "r" : "-");
    printf((S_IWGRP & file_stat.st_mode) ? "w" : "-");
    printf((S_IXGRP & file_stat.st_mode) ? "x" : "-");
    printf((S_IROTH & file_stat.st_mode) ? "r" : "-");
    printf((S_IWOTH & file_stat.st_mode) ? "w" : "-");
    printf((S_IXOTH & file_stat.st_mode) ? "x" : "-");
    printf(" ");

    // number of links
    printf("%*lu ", (int)allign.max_link, file_stat.st_nlink);

    // Username & Group name
    struct passwd *pw = getpwuid(file_stat.st_uid);
    struct group *gr = getgrgid(file_stat.st_gid);
    printf("%-*s %-*s ", (int)allign.max_user, pw->pw_name,
           (int)allign.max_group, gr->gr_name);

    // size of the file
    printf("%*ld ", (int)allign.max_size, file_stat.st_size);

    // This is the time of last modification of file data.
    char timebuf[20];
    struct tm *tm_info = localtime(&file_stat.st_mtime);

    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm_info);
    printf("%s ", timebuf);

    printf("%s\n", file);
  } else {
    printf("%s ", file);
  }
}

// to sort string in the output
int compare(const void *a, const void *b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

void calculate_alignment(const char *directory, struct dirent *entry) {
  struct stat file_stat;
  char full_path[PATH_MAX];
  create_path_string(full_path, sizeof(full_path), directory, entry->d_name);
  if (stat(full_path, &file_stat) == -1) {
    perror(strerror(errno));
    exit(EXIT_FAILURE);
  }

  struct passwd *pw = getpwuid(file_stat.st_uid);
  struct group *gt = getgrgid(file_stat.st_gid);

  size_t user_len = pw ? strlen(pw->pw_name) : 1;
  size_t group_len = gt ? strlen(gt->gr_name) : 1;
  size_t link_len = snprintf(NULL, 0, "%ld", file_stat.st_nlink);
  size_t size_len = snprintf(NULL, 0, "%ld", file_stat.st_size);

  if (user_len > allign.max_user) {
    allign.max_user = user_len;
  }
  if (group_len > allign.max_group) {
    allign.max_group = group_len;
  }
  if (link_len > allign.max_link) {
    allign.max_link = link_len;
  }
  if (size_len > allign.max_size) {
    allign.max_size = size_len;
  }
}

/*
 @return number of proccesed files
*/
size_t process_directory(const char *directory, DIR *dir, char **filenames) {
  size_t count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (lsopts.debug) { // Debugging
      printf("DEBUG: Found file -> %s\n", entry->d_name);
    }
    if (!lsopts.show_hidden && entry->d_name[0] == '.') {
      continue;
    }

    if (count < MAX_FILES) {
      if (!(filenames[count] = strdup(entry->d_name))) {
        perror("strdup");
        exit(EXIT_FAILURE);
      }
      ++count;
      calculate_alignment(directory, entry);
    } else {
      printf("Too many files in directory. Max count of files is %d\n",
             MAX_FILES);
      break;
    }
  }
  return count;
}

void ls(const char *directory) {
  DIR *dir;
  if (!(dir = opendir(directory))) {
    perror(strerror(errno));
    exit(EXIT_FAILURE);
  }

  char *filenames[MAX_FILES];
  size_t count = process_directory(directory, dir, filenames);

  closedir(dir);

  qsort(filenames, count, sizeof(char *), compare);

  for (size_t i = 0; i < count; ++i) {
    char full_path[PATH_MAX];

    create_path_string(full_path, sizeof(full_path), directory, filenames[i]);
    if (lsopts.debug) { // Debugging
      printf("DEBUG: full_path = '%s'\n", full_path);
    }
    print_info(full_path);

    free(filenames[i]);
  }
}

void print_help(char *programName) {
  printf("Usage: %s [OPTION]... [FILE]...\n", programName);
  printf(
      "List information about the FILEs (the current directory by default).\n");
  printf("Sort entries alphabetically if none of -cftuvSUX nor --sort is "
         "specified.\n");
  printf("\n");
  printf("Mandatory arguments to long options are mandatory for short options "
         "too.\n");
  printf(
      "  -a, --all                  do not ignore entries starting with .\n");
  printf("  -l                         use a long listing format\n");
  printf("  -h, --help                 display this help and exit\n");
  printf("\n");
}

void handle_options(int argc, char **argv) {
  // In other words we can have -a, -l or -h as an option.
  // Other symbols will cause an error
  const char *possible_options = "alhd";
  int opt;

  while ((opt = getopt(argc, argv, possible_options)) != -1) {
    switch (opt) {
    case 'a': {
      lsopts.show_hidden = true;
      break;
    }
    case 'l': {
      lsopts.long_format = true;
      break;
    }
    case 'h': {
      print_help(argv[0]);
      exit(EXIT_SUCCESS);
    }
    case 'd': {
      lsopts.debug = true;
    }
    }
  }
}

int main(int argc, char **argv) {
  handle_options(argc, argv);

  // After getopt() 'optind' points to first argument, that isn't an option
  // No path is provided. List current directory
  if (optind == argc) {
    ls(".");
  } else {
    // Several paths are provided. List all
    for (int i = optind; i < argc; ++i) {
      printf("%s:\n", argv[i]);
      ls(argv[i]);
      printf("\n");
    }
  }

  return EXIT_SUCCESS;
}