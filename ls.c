#include <assert.h>
#include <bits/getopt_core.h>
#include <dirent.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <linux/limits.h>
#include <pwd.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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
  bool recursive;
  bool debug;
} lsopts;

static struct allignment {
  size_t max_link;
  size_t max_user;
  size_t max_group;
  size_t max_size;
} allign;

void create_path_string(char *str, size_t size, const char *directory,
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

void calculate_alignment(const FTSENT *entry) {
  struct stat *file_stat = entry->fts_statp;
  if (!file_stat) {
    fprintf(stderr, "Error: fts_statp is NULL\n");
    return;
  }

  struct passwd *pw = getpwuid(file_stat->st_uid);
  struct group *gt = getgrgid(file_stat->st_gid);

  size_t user_len = pw && pw->pw_name ? strlen(pw->pw_name) : 1;
  size_t group_len = gt && gt->gr_name ? strlen(gt->gr_name) : 1;
  size_t link_len = snprintf(NULL, 0, "%ld", file_stat->st_nlink);
  size_t size_len = snprintf(NULL, 0, "%ld", file_stat->st_size);

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

void process_directory(char *dir, uint8_t fts_options, char **filenames,
                       size_t *count) {
  char *dirs[] = {dir, NULL};
  FTS *fts = fts_open(dirs, fts_options, NULL);
  if (!fts) {
    perror(strerror(errno));
    exit(EXIT_FAILURE);
  }

  FTSENT *entry = NULL;

  while ((entry = fts_read(fts)) != NULL) {
    if (*count >= MAX_FILES) {
      printf("Too many files in directory. Max count of files is %d\n",
             MAX_FILES);
      break;
    }

    switch (entry->fts_info) {
    case FTS_D: {
      if (!lsopts.recursive && entry->fts_level > 0) {
        fts_set(fts, entry, FTS_SKIP);
        continue;
      }
      break;
    }
    case FTS_ERR: {
      fprintf(stderr, "Error reading directory: %s\n", entry->fts_path);
      fts_close(fts);
      exit(EXIT_FAILURE);
    }
    case FTS_NS: {
      if (lsopts.debug) {
        fprintf(stderr, "Error: %s\n", entry->fts_path);
      }
      break;
    }
    case FTS_DP: {
      continue;
    }
    default:
      break;
    }

    if (!(filenames[*count] = strdup(entry->fts_name))) {
      perror("strdup");
      exit(EXIT_FAILURE);
    }

    if (lsopts.long_format) {
      calculate_alignment(entry);
    }

    ++*count;
  }

  fts_close(fts);
}

void ls(char **paths, size_t path_count) {
  uint8_t fts_options = FTS_NOCHDIR | FTS_PHYSICAL;

  if (lsopts.show_hidden) {
    fts_options |= FTS_SEEDOT;
  }

  if (!lsopts.long_format) {
    fts_options |= FTS_NOSTAT;
  }

  char ***filenames = malloc(path_count * sizeof(char **));
  if (!filenames) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  for (size_t i = 0; i < path_count; ++i) {
    filenames[i] = malloc(MAX_FILES * sizeof(char *));
    if (!filenames[i]) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    memset(filenames[i], 0, MAX_FILES * sizeof(char *)); // Initialize to NULL
  }
  size_t *count = calloc(path_count, sizeof(size_t));

  for (int i = 0; i < path_count; ++i) {
    process_directory(paths[i], fts_options, filenames[i], &count[i]);
  }

  for (int i = 0; i < path_count; ++i) {
    qsort(filenames[i], count[i], sizeof(char *), compare);
  }

  for (size_t path_index = 0; path_index < path_count; ++path_index) {
    for (size_t filenames_index = 0; filenames_index < count[path_index];
         ++filenames_index) {
      char full_path[PATH_MAX];
      create_path_string(full_path, sizeof(full_path), paths[path_index],
                         filenames[path_index][filenames_index]);
      print_info(full_path);
      free(filenames[path_index][filenames_index]);
    }
    free(filenames[path_index]);
  }

  free(filenames);
  free(count);

  if (!lsopts.long_format) {
    printf("\n");
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
  printf("  -r                         go through every directory recursevily");
  printf("  -h, --help                 display this help and exit\n");
  printf("\n");
}

void handle_options(int argc, char **argv) {
  // In other words we can have -a, -l or -h as an option.
  // Other symbols will cause an error
  const char *possible_options = "alhd";
  int opt = 0;

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
    case 'r': {
      lsopts.recursive = true;
      break;
    }
    case 'd': {
      lsopts.debug = true;
      break;
    }
    }
  }
}

int main(int argc, char **argv) {
  handle_options(argc, argv);

  // After getopt() 'optind' points to first argument, that isn't an option
  // No path is provided. List current directory
  if (optind == argc) {
    char *default_path = ".";
    ls(&default_path, 1);

  } else {
    size_t path_count = argc - optind;
    char **paths = malloc(path_count * sizeof(char *));

    for (int i = 0; i < path_count; ++i) {
      paths[i] = argv[optind + i];
    }
    ls(paths, path_count);
    free(paths);
  }

  return EXIT_SUCCESS;
}
