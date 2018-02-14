/*
 * Creator: Naman Dixit
 * Created: 08:34:28 AM IST August 13, 2017
 * Notice: (C) Copyright 2017 by Naman Dixit. All Rights Reserved.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdnoreturn.h>
#include <assert.h>

#include "defs.h"
#include "types.h"

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/limits.h>

typedef enum ClogPostType {
    ClogPostType_UNKNOWN,
    ClogPostType_ARTICLE,
    ClogPostType_BLOG,
    ClogPostType_PROJECT,
    ClogPostType_CODE
} ClogPostType;

__attribute__((__format__ (__printf__, 1, 2)))
internal_function
noreturn
void clogError (Char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n\n\n");

    abort();
}


internal_function
Bool32 clogIsClogFile(Char *path)
{
    Char *dot = strrchr(path, '.');

    if (strcmp(dot + 1, "clog") == 0) {
        return true;
    } else {
        return false;
    }
}

internal_function
Char *clogStringRSearch (Char *haystack, Char *needle)
{
    Char *result = NULL;

    Char *found = haystack;

    while (1) {
        found = strstr(found, needle);

        if (found == NULL) {
            break;
        }

        result = found;
        found++; /* Since found will be pointing to the first match and the same substring
                  * will keep getting matched again and again.
                  */
    }

    return result;
}

__attribute__((__format__ (__printf__, 2, 3)))
internal_function
void clogCat (Char *str, Char *format, ...)
{
    Size l = strlen(str);

    va_list args;
    va_start(args, format);
    vsprintf(str + l, format, args);
    va_end(args);
}

internal_function
void clogInsertCharInHTML (Char* str, Char c)
{
    if (c == '>') {
        clogCat(str, "&gt;");
    } else if (c == '<') {
        clogCat(str, "&lt;");
    } else if (c == ';') {
        clogCat(str, "&semi;");
    } else if (c == '&') {
        clogCat(str, "&amp;");
    } else if (c == '"') {
        clogCat(str, "&quot;");
    } else if (c == '`') {
        clogCat(str, "&grave;");
    } else if (c == '#') {
        clogCat(str, " &num;");
    } else if (c == '^') {
        clogCat(str, "&Hat;");
    } else {
        clogCat(str, "%c", c);
    }
}

internal_function
void clogParse (Char *clog_content, Char *html_content)
{ // Actual stuff
    Char *c = clog_content;
    Char *h = html_content;

#define COND(ch) ((c[0] == (ch)) && (c[1] == (ch)))
#define BODY(ch, tag, class) do {                   \
        c += 2;                                     \
                                                    \
        clogCat(h, "<" tag ">");                    \
        clogCat(h, "<span class=\"" class "\">");   \
                                                    \
        while (!(c[0] == ch && c[1] == ch)) {       \
            clogInsertCharInHTML(h, c[0]);          \
            c++;                                    \
        }                                           \
                                                    \
        clogCat(h, "</span>");                      \
        clogCat(h, "</" tag ">");                   \
                                                    \
        c += 2;                                     \
    } while (0)

    while (*c) {
        if (c[0] == '\\') {
            clogInsertCharInHTML(h, c[1]);
            c += 2;
        } else if (COND('*')) { // bold
            BODY('*', "strong", "bold");
        } else if (COND('_')) { // italics
            BODY('_', "em", "italics");
        } else if (COND('~')) {
            BODY('~', "s", "strike-through");
        } else if (COND('`')) {
            BODY('`', "code", "inline-code");
        } else if (c[0] == '#' && c[1] == '#') {
            c += 2;

            while (c[0] == ' ') {
                c++;
            }

            clogCat(h, "<h2><span class=\"heading\">");
            while (c[0] != '\n') {
                clogInsertCharInHTML(h, c[0]);
                c++;
            }
            clogCat(h, "</span></h2>");
        } else if (COND('[')) {
            c += 2;

            clogCat(h, "<a href=\"");
            Char *text = c;

            while (!(c[0] == ']' && c[1] == ']')) {
                c++;
            }
            c += 2;

            assert(c[0] == '<');
            c++;

            while (c[0] != '>') {
                clogInsertCharInHTML(h, c[0]);
                c++;
            }

            c++;

            clogCat(h, "\">");

            while (text[0] != ']') {
                clogInsertCharInHTML(h, text[0]);
                text++;
            }

            clogCat(h, "</a>");
        } else if (COND('{')) {
            c += 2;

            clogCat(h, "<img alt=\"");

            while (!COND('}')) {
                clogInsertCharInHTML(h, c[0]);
                c++;
            }

            c += 2;

            assert(c[0] == '<');
            c++;

            clogCat(h, "\" src=\"");

            while (c[0] == '}') {
                clogInsertCharInHTML(h, c[0]);
                c++;
            }

            c++;

            clogCat(h, "\">");
        } else if (COND('\n')) {
            clogCat(h, "<p>");
            c++;
        } else {
            clogInsertCharInHTML(h, c[0]);
            c++;
        }
    }

#undef COND
#undef BODY
}

internal_function
void clogTitleFromFilename (Char *dest, Char *from, Char *to)
{
    for (Char *p = from; p != to; p++) {
        if (p[0] == '_') {
            if (p[1] == '_') {
                clogInsertCharInHTML(dest, '_');
                p++; // For loop will increment once more
            } else {
                clogInsertCharInHTML(dest, ' '); // For loop will increment
            }
        } else {
            clogInsertCharInHTML(dest, p[0]);
        }
    }
}

internal_function
void clogHeader (Char *html_content, Char *clog_path)
{
    clogCat(html_content,
            "<!doctype html>\n"
            "\n"
            "<html lang=\"en\">\n"
            "  <head>\n"
            "    <meta charset=\"utf-8\">\n"
            "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
            "\n"
            "    <title>Naman Dixit</title>\n"
            "\n"
            "    <link href=\"https://fonts.googleapis.com/css?family=Slabo+27px|Abel|PT+Mono\"\n"
            "          rel=\"stylesheet\">\n"
            "\n"
            "    <link href=\"/res/css/reset.css\" rel=\"stylesheet\" type=\"text/css\">\n"
            "    <link href=\"/res/css/style.css\" rel=\"stylesheet\" type=\"text/css\">\n"
            "\n"
            "  </head>\n"
            "\n"
            "  <body>\n"
            "    <header>\n"
            "      <div class=\"site-name\"><a href=\"/index.html\">Naman Dixit</a></div>\n"
            "\n"
            "      <nav class=\"site-map\">\n"
            "        <a href=\"/posts/projects/index.html\"  class=\"projects\" >Projects</a> |\n"
            "        <a href=\"/posts/articles/index.html\"  class=\"articles\" >Articles</a> |\n"
            "        <a href=\"/posts/blog/index.html\"      class=\"blog\"     >Blog</a> |\n"
            "        <a href=\"/posts/code/index.html\"      class=\"code\"     >Code</a>\n"
            "      </nav>\n"
            "    </header>\n"
            "\n"
            "    <main>\n"
            "      <div class=\"content\">\n"
            "        <h1><span class=\"page-title\">");

    Char *title_init = clogStringRSearch(clog_path, "/") + 1;
    Char *title_finish = clogStringRSearch(clog_path, ".clog");

    clogTitleFromFilename(html_content, title_init, title_finish);

    clogCat(html_content,
            "</span></h1>\n"
            "\n"
            "\n"
            "\n");

    clogCat(html_content,
            "<!-- CONTENT STARTS HERE -->\n");
}

internal_function
void clogFooterCC0 (Char *html_content)
{
    clogCat(html_content,
            "\n"
            "<!-- CONTENT ENDS HERE -->\n");

    clogCat(html_content,
            "\n"
            "\n"
            "\n"
            "      </div>\n"
            "    </main>\n");

    clogCat(html_content,
            "    <footer>\n"
            "      <div class=\"cc0-license\">\n"
            "        <p xmlns:dct=\"http://purl.org/dc/terms/\"\n"
            "           xmlns:vcard=\"http://www.w3.org/2001/vcard-rdf/3.0#\">\n"
            "          <a rel=\"license\" class=\"cc0-logo-link\"\n"
            "             href=\"http://creativecommons.org/publicdomain/zero/1.0/\">\n"
            "            <img src=\"https://mirrors.creativecommons.org/presskit/buttons/88x31/svg/cc-zero.svg\"\n"
            "                 style=\"border-style: none;\"\n"
            "                 alt=\"CC0\" class=\"cc0-logo\"/>\n"
            "          </a>\n"
            "        </p>\n"
            "\n"
            "        <p class=\"cc0-text\">\n"
            "          To the extent possible under law,\n"
            "          <a rel=\"dct:publisher\"\n"
            "             href=\"/index.html\">\n"
            "            <span property=\"dct:title\">Naman Dixit</span></a>\n"
            "          has waived all copyright and related or neighboring rights to\n"
            "          this work.\n"
            "          This work is published from:\n"
            "          <span property=\"vcard:Country\" datatype=\"dct:ISO3166\"\n"
            "                content=\"IN\" about=\"namandixit.github.io\">\n"
            "            India</span>.\n"
            "        </p>\n"
            "      </div>\n"
            "    </footer>\n"
            "\n"
            "  </body>\n"
            "</html>\n");
}

internal_function
void clogFooterCode (Char *html_content)
{
    clogCat(html_content,
            "\n"
            "<!-- CONTENT ENDS HERE -->\n");

    clogCat(html_content,
            "\n"
            "\n"
            "\n"
            "      </div>\n"
            "    </main>\n");

    clogCat(html_content,
            "    <footer>\n"
            "      <div class=\"zlib-license\">\n"
            "        Copyright (C) 2018 Naman Dixit\n"
            "        <br>\n"
            "        Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.\n"
            "        <br>\n"
            "        DISCLAIMER: THE WORKS ARE WITHOUT WARRANTY.\n"
            "      </div>\n"
            "    </footer>\n"
            "\n"
            "  </body>\n"
            "</html>\n");
}

internal_function
Char* clogBlogToHTML (Char *clog_path)
{
    FILE *clog_file = fopen(clog_path, "r");
    fseek(clog_file, 0, SEEK_END);
    Size clog_size = (Size)ftell(clog_file);
    fseek(clog_file, 0, SEEK_SET);

    Char *clog_content = calloc(clog_size + 1, 1);
    fread(clog_content, 1, clog_size, clog_file);

    Char *html_content = calloc(MiB(100), 1);

    clogHeader(html_content, clog_path);
    clogParse(clog_content, html_content);
    clogFooterCC0(html_content);

    fclose(clog_file);
    free(clog_content);

    return html_content;
}

internal_function
Char* clogArticleToHTML (Char *clog_path)
{
    FILE *clog_file = fopen(clog_path, "r");
    fseek(clog_file, 0, SEEK_END);
    Size clog_size = (Size)ftell(clog_file);
    fseek(clog_file, 0, SEEK_SET);

    Char *clog_content = calloc(clog_size + 1, 1);
    fread(clog_content, 1, clog_size, clog_file);

    Char *html_content = calloc(MiB(100), 1);

    clogHeader(html_content, clog_path);
    clogParse(clog_content, html_content);
    clogFooterCC0(html_content);

    fclose(clog_file);
    free(clog_content);

    return html_content;
}

// TODO:
internal_function
Char* clogCodeToHTML (Char *clog_path)
{
    FILE *clog_file = fopen(clog_path, "r");
    fseek(clog_file, 0, SEEK_END);
    Size clog_size = (Size)ftell(clog_file);
    fseek(clog_file, 0, SEEK_SET);

    Char *clog_content = calloc(clog_size + 1, 1);
    fread(clog_content, 1, clog_size, clog_file);

    Char *html_content = calloc(MiB(100), 1);

    clogHeader(html_content, clog_path);
    clogParse(clog_content, html_content);
    clogFooterCode(html_content);

    fclose(clog_file);
    free(clog_content);

    return html_content;
}

internal_function
void clogWriteHTML (Char *html_path, Char *html_content)
{
    FILE *html_file = fopen(html_path, "w");
    fwrite(html_content, strlen(html_content), 1, html_file);
    fclose(html_file);
    free(html_content);

    return;
}

internal_function
void clogConvertClog (Char *clog_path, Char *html_path, ClogPostType clog_type)
{
    Char *html_content = NULL;

    switch (clog_type) {
        case ClogPostType_ARTICLE: {
            html_content = clogArticleToHTML(clog_path);
        } break;

        case ClogPostType_BLOG: {
            html_content = clogBlogToHTML(clog_path);
        } break;

        case ClogPostType_CODE: {
            html_content = clogCodeToHTML(clog_path);
        } break;

        case ClogPostType_UNKNOWN:
        case ClogPostType_PROJECT: {
            clogError("Invalid type");
        }
    }
    clogWriteHTML(html_path, html_content);

    return;
}

internal_function
void clogAddEntry (Char *dir_path, Char *clog_path)
{
    Char index_path[PATH_MAX] = {};
    strcpy(index_path, dir_path);
    strcat(index_path, "/index.html");

    FILE *index_file = fopen(index_path, "r");
    fseek(index_file, 0, SEEK_END);
    Size index_size = (Size)ftell(index_file);
    fseek(index_file, 0, SEEK_SET);

    Char *index_content = calloc(index_size + 1, 1);
    fread(index_content, 1, index_size, index_file);

    Char *index_content_new = calloc(index_size + MiB(1), 1);

    Char *marker = "<!-- INDEX BEGINS HERE -->\n\n";
    Char *until = strstr(index_content, marker);
    until += strlen(marker);
    strncpy(index_content_new, index_content, (Size)(until - index_content));

    Char *title_init = clogStringRSearch(clog_path, "/") + 1;
    Char *title_finish = clogStringRSearch(clog_path, ".clog");

    clogCat(index_content_new,
            "\n<div class=\"entry\"><a href=\"./%s.html\">",
            title_init);

    clogTitleFromFilename(index_content_new, title_init, title_finish);

    clogCat(index_content_new, "</a></div>");

    strcat(index_content_new, until);

    clogWriteHTML(index_path, index_content_new);

    free(index_content);

    fclose(index_file);

    return;
}


internal_function
void clogProcessDirectory (Char *dir_path, ClogPostType clog_type)
{
    Char *dir_posts[] = {dir_path, NULL};
    FTS *fts_posts = fts_open(dir_posts, FTS_PHYSICAL | FTS_NOCHDIR, NULL);

    if (fts_posts == NULL) {
        clogError("ERROR: fts_open: Can't open %s directory\n", dir_posts[0]);
    }

    while (true) {
        FTSENT *child = fts_read(fts_posts);
        if (child == NULL) {
            if (errno == 0) {
                break; // No more files
            } else {
                clogError("ERROR: fts_read: An error occured\n");
            }
        }

        if(!(child->fts_info & FTS_F)) { // Not a file
            continue;
        }

        if(!clogIsClogFile(child->fts_path)) { // Not a clog file
            continue;
        }

        fprintf(stderr, "Processing - %-50s: ",
                clogStringRSearch(child->fts_path, "/posts/") + strlen("/posts/"));

        FTSENT *clog = child;
        Char *clog_path = clog->fts_path;
        Size clog_path_length = strlen(clog_path);
        Uint html_ext_length = strlen(".html") + 1;
        Char *html_path = malloc(clog_path_length + html_ext_length);
        strcpy(html_path, clog_path);
        strcat(html_path, ".html");

        struct stat html_stat = {};
        int html_stat_code = stat(html_path, &html_stat);

        if (html_stat_code < 0) { // Error
            if (errno == ENOENT) { // HTML file doesn't exist
                fprintf(stderr, "NO HTML FILE\n");
                clogConvertClog(clog_path, html_path, clog_type);
                clogAddEntry(dir_path, clog_path);
            } else { // Some other error
                clogError("ERROR(%d): stat failed for %s\n",
                          errno, html_path);
            }

            continue;
        }

        // HTML file exists
        struct stat clog_stat = {};
        int clog_stat_code = stat(clog_path, &clog_stat);

        if (clog_stat_code < 0) { // clog file gave error, weird!
            clogError("ERROR: stat failed for %s\n",
                      clog_path);
        }

        if (clog_stat.st_mtim.tv_sec > html_stat.st_mtim.tv_sec) { // clog file is newer
            fprintf(stderr, "OLD HTML FILE\n");
            clogConvertClog(clog_path, html_path, clog_type);
            continue;
        }

        // clog file is older than HTML
        fprintf(stderr, "Nothing to do\n");
    }
}

Sint main (Sint argc, Char *argv[])
{
    (void)argc;
    (void)argv;

    Char cwd[PATH_MAX] = {0};
    getcwd(cwd, PATH_MAX);
    strcat(cwd, "/posts");

    { // Articles
        Char article_dir[PATH_MAX] = {0};
        strcpy(article_dir, cwd);
        strcat(article_dir, "/articles");
        clogProcessDirectory(article_dir, ClogPostType_ARTICLE);
    }

    { // Blog
        Char blog_dir[PATH_MAX] = {0};
        strcpy(blog_dir, cwd);
        strcat(blog_dir, "/blog");
        clogProcessDirectory(blog_dir, ClogPostType_BLOG);
    }

    { // Code
        Char code_dir[PATH_MAX] = {0};
        strcpy(code_dir, cwd);
        strcat(code_dir, "/code");
        clogProcessDirectory(code_dir, ClogPostType_CODE);
    }

    return 0;
}
