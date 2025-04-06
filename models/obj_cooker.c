// vim: sw=2 ts=2 expandtab smartindent

/**
 * turn an obj file into a simple binary format or a header containing that data
 **/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <math.h>

typedef struct {
  struct { float x, y, z; } pos;
  struct { uint8_t r, g, b, a; } color;
  struct { float x, y, z; } normal;
} gl_geo_Vtx;
typedef struct { uint16_t a, b, c; } gl_Tri;

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

  size_t vtx_count = 0;
  size_t tri_count = 0;
  {
    char *newline_ctx;
    char *line = strtok_r(obj, "\n", &newline_ctx);
    do {
      vtx_count += line[0] == 'v';
      tri_count += line[0] == 'f';
    } while((line = strtok_r(NULL, "\n", &newline_ctx)));
  }

  gl_geo_Vtx *vtxs = calloc(vtx_count, sizeof(gl_geo_Vtx));
  gl_Tri     *tris = calloc(tri_count, sizeof(gl_Tri));
  size_t vtx_idx = 0;
  size_t tri_idx = 0;
  size_t vtx_idx_normal = 0;

  char model_name[999] = {0};
  char *newline_ctx;
  char *line = strtok_r(obj, "\n", &newline_ctx);
  do {
    printf("%s\n", line);

    char *space_ctx;
    char *word = strtok_r(line, " ", &space_ctx);
    char op = word[0];
    int op_vrt    = op == 'v' && word[1] == ' ';
    int op_normal = op == 'v' && word[1] == 'n';

    int index = 0;
    do {
      puts(word);

      if (op == 'o' && index == 1) {
        strlcpy(model_name, word, sizeof(model_name));
      }

      if (op_vrt && index == 0) vtx_idx++;
      if (op_vrt && index == 1) vtxs[vtx_idx].pos.x = strtof(word, NULL);
      if (op_vrt && index == 2) vtxs[vtx_idx].pos.y = strtof(word, NULL);
      if (op_vrt && index == 3) vtxs[vtx_idx].pos.z = strtof(word, NULL);
      if (op_vrt && index == 4) vtxs[vtx_idx].color.r = srgb2linear(strtof(word, NULL));
      if (op_vrt && index == 5) vtxs[vtx_idx].color.g = srgb2linear(strtof(word, NULL));
      if (op_vrt && index == 6) vtxs[vtx_idx].color.b = srgb2linear(strtof(word, NULL));

      if (op_normal && index == 0) vtx_idx_normal++;
      if (op_normal && index == 1) vtxs[vtx_idx_normal].normal.x = strtof(word, NULL);
      if (op_normal && index == 2) vtxs[vtx_idx_normal].normal.y = strtof(word, NULL);
      if (op_normal && index == 3) vtxs[vtx_idx_normal].normal.z = strtof(word, NULL);

      if (op == 'f' && index == 0) tri_idx++;
      if (op == 'f' && index == 1) tris[tri_idx].a = strtol(word, NULL, 10) - 1;
      if (op == 'f' && index == 2) tris[tri_idx].b = strtol(word, NULL, 10) - 1;
      if (op == 'f' && index == 3) tris[tri_idx].c = strtol(word, NULL, 10) - 1;

      index++;
    } while ((word = strtok_r(NULL, " ", &space_ctx)));

    // puts("\n");
  } while ((line = strtok_r(NULL, "\n", &newline_ctx)));

  fprintf(out, "gl_geo_Vtx model_vtx_%s[] = {\n", model_name);

  for (int i = 0; i < vtx_count; i++) {
    fprintf(
      out,
      "  { { %9f, %9f, %9f }, { %4d, %4d, %4d }, { %9f, %9f, %9f } },\n",
      vtxs[i].pos.x,
      vtxs[i].pos.y,
      vtxs[i].pos.z,
      vtxs[i].color.r,
      vtxs[i].color.g,
      vtxs[i].color.b,
      vtxs[i].normal.x,
      vtxs[i].normal.y,
      vtxs[i].normal.z
    );
  }

  fprintf(out, "};\n\n");
  fprintf(out, "gl_Tri model_tri_%s[] = {\n", model_name);

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
