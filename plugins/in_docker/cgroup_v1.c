/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2015-2023 The Fluent Bit Authors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <fluent-bit/flb_log.h>

#include <dirent.h>
#include <string.h>
#include "docker.h"

/* This method returns list of currently running docker ids. */
static struct mk_list *get_active_dockers()
{
    DIR *dp;
    struct dirent *ep;
    struct mk_list *list;

    list = flb_malloc(sizeof(struct mk_list));
    if (!list) {
        flb_errno();
        return NULL;
    }
    mk_list_init(list);

    dp = opendir(DOCKER_CGROUP_CPU_DIR);
    if (dp != NULL) {
        ep = readdir(dp);

        while(ep != NULL) {
            if (ep->d_type == OS_DIR_TYPE) {
                if (strcmp(ep->d_name, CURRENT_DIR) != 0
                    && strcmp(ep->d_name, PREV_DIR) != 0
                    && strlen(ep->d_name) == DOCKER_LONG_ID_LEN) { /* precautionary check */

                    docker_info *docker = in_docker_init_docker_info(ep->d_name);
                    mk_list_add(&docker->_head, list);
                }
            }
            ep = readdir(dp);
        }
        closedir(dp);
    }

    return list;
}

int in_docker_set_cgroup_api_v1(struct cgroup_api *api)
{
    api->cgroup_version = 1;
    api->get_active_docker_ids = get_active_dockers;

    return 0;
}
