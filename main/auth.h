#pragma once
#include <stdbool.h>
void auth_init(void);                                                   /* load admin.csv (or create the default admin) */
bool auth_login(const char *user, const char *pass, int *wait_s);       /* false + wait_s>0 means locked out */
bool auth_set_password(const char *user, const char *new_pass);
