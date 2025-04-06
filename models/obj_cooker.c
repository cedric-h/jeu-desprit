// vim: sw=2 ts=2 expandtab smartindent

/**
 * turn an obj file into a simple binary format or a header containing that data
 **/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <math.h>

/* takes a color channel in srgb in 0..1 and returns the same value in linear 0..255 */
uint8_t srgb2linear(float srgb) {
  return roundf(powf(srgb, 2.2f) * 255.0f);
}

int cook(char *file_name) {

  FILE *out;
  {
    char buf[999] = {0};
    snprintf(buf, sizeof(buf), "./include/%s.h", file_name);
    out = fopen(buf, "w");
    if (out == NULL) { perror("Failed to create .h file"); return 1; }
  }

  /* read file contents into obj */
  char *obj;
  {
    FILE *file;
    {
      char buf[999] = {0};
      snprintf(buf, sizeof(buf), "./obj/%s.obj", file_name);
      file = fopen(buf, "r");
      if (file == NULL) { perror("Failed to open .obj file"); return 1; }
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *)malloc(file_size + 1);  /* +1 for the null terminator */
    if (buffer == NULL) { perror("Failed to allocate memory"); return 1; }

    /* read the entire file into the buffer */
    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) { perror("Failed to read the entire file"); return 1; }

    /* null-terminate the buffer */
    buffer[bytes_read] = '\0';

    fclose(file);

    obj = buffer;
  }

  // fprintf(out, "#include <stdint.h>\n\n");

  // fprintf(out, "typedef struct {\n");
  // fprintf(out, "  struct { float x, y, z; } pos;\n");
  // fprintf(out, "  struct { uint8_t r, g, b, a; } color;\n");
  // fprintf(out, "} gl_geo_Vtx;\n\n");

  char model_name[999] = {0};
  char *newline_ctx;
  char *line = strtok_r(obj, "\n", &newline_ctx);
  do {
    // printf("%s\n", line);

    char *space_ctx;
    char *word = strtok_r(line, " ", &space_ctx);
    char op = word[0];

    int index = 0;
    do {
      // printf("%s\n", word);

      if (op == 'o' && index == 1) {
        fprintf(out, "gl_geo_Vtx model_vtx_%s[] = {\n", word);
        strlcpy(model_name, word, sizeof(model_name));
      }

      if (op == 'v' && index == 0) fprintf(out, "  {");
      if (op == 'v' && index == 1) fprintf(out, " { %10f,",    strtof(word, NULL));
      if (op == 'v' && index == 2) fprintf(out,   " %10f,",    strtof(word, NULL));
      if (op == 'v' && index == 3) fprintf(out,   " %10f }, ", strtof(word, NULL));
      if (op == 'v' && index == 4) fprintf(out, " { %3d,",   srgb2linear(strtof(word, NULL)));
      if (op == 'v' && index == 5) fprintf(out,   " %3d,",   srgb2linear(strtof(word, NULL)));
      if (op == 'v' && index == 6) fprintf(out,   " %3d } ", srgb2linear(strtof(word, NULL)));

      /* abusing the fact that s always becomes between v section and f section in blender export objs */
      if (op == 's' && index == 0) {
        fprintf(out, "};\n\n");
        fprintf(out, "gl_Tri model_tri_%s[] = {\n", model_name);
      }

      if (op == 'f' && index == 1) fprintf(out, "  { %5ld,",     strtol(word, NULL, 10) - 1);
      if (op == 'f' && index == 2) fprintf(out,    " %5ld,",     strtol(word, NULL, 10) - 1);
      if (op == 'f' && index == 3) fprintf(out,    " %5ld },\n", strtol(word, NULL, 10) - 1);

      index++;
    } while ((word = strtok_r(NULL, " ", &space_ctx)));

    if (op == 'v') fprintf(out, "},\n");

    // puts("\n");
  } while ((line = strtok_r(NULL, "\n", &newline_ctx)));

  fprintf(out, "};\n");

  free(obj);
  fclose(out);

  return 0;
}

int main(int n_args, char **args) {
  if (n_args == 1) {
    puts("No argument provided, cooking obj/* ...");

    DIR *dir = opendir("./obj/");
    struct dirent *entry;
    if (dir == NULL) { perror("opendir ./obj/"); return 1; }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        strtok(entry->d_name, ".");
        printf("Cooking ./obj/%s.obj into ./include/%s.h ... \n", entry->d_name, entry->d_name);
        cook(entry->d_name);
    }

    closedir(dir);
    puts("done!");
    return 0;
  }

  if (n_args == 2) {
    strtok(args[1], ".");
    printf("Cooking ./obj/%s.obj into ./include/%s.h ... \n", args[1], args[1]);
    return cook(args[1]);
  }

  perror("Supported number of arguments: 0, 1");
  return 0;
}
