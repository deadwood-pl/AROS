/*
    Copyright (C) 1995-2014, The AROS Development Team. All rights reserved.

    Function to write module_autoinit.c. Part of genmodule.
*/

#include "genmodule.h"

void writeautoinit(struct config *cfg, int is_rel)
{
    FILE *out;
    char line[256], *banner;
    struct stringlist *linelistit;

    snprintf(line, 255, "%s/%s_%sautoinit.c", cfg->libgendir, cfg->modulename, is_rel ? "rel" : "");
    out = fopen(line, "w");

    if (out==NULL)
    {
        perror(line);
        exit(20);
    }

    /* Write the code to be added to startup provided in the config file */
    if (!is_rel) {
        for (linelistit = cfg->startuplines; linelistit != NULL; linelistit = linelistit->next)
        {
            fprintf(out, "%s\n", linelistit->s);
        }
    }

    banner = getBanner(cfg);
    fprintf(out, "%s\n", banner);
    freeBanner(banner);

    if (!(cfg->options & OPTION_NOINCLUDES))
        fprintf(out, "#include <proto/%s.h>\n", cfg->includename);
    fprintf(out,
            "#include <aros/symbolsets.h>\n"
            "\n"
            "AROS_%sLIBSET(\"%s.%s\", %s, %s)\n",
            is_rel ? "REL" : "",
            cfg->includename, cfg->suffix,
            cfg->libbasetypeptrextern, cfg->libbase
    );

    fprintf(out,
            "AROS_IMPORT_ASM_SYM(int, dummy, __include%slibrarieshandling);\n",
            is_rel ? "rel" : ""
    );

    fclose(out);
}
