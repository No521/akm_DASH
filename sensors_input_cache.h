/*
 * Copyright (C) 2012 Sony Mobile Communications AB.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SENSORS_INPUT_CACHE_H_
#define SENSORS_INPUT_CACHE_H_

#define INPUT_EVENT_DIR      "/dev/input/"
#define INPUT_EVENT_BASENAME "event"
#define INPUT_EVENT_PATH     INPUT_EVENT_DIR INPUT_EVENT_BASENAME
#define MAX_INT_STRING_SIZE  sizeof("4294967295")

struct sensors_input_cache_entry_t {
	int nr;
	char dev_name[32];
	char event_path[sizeof(INPUT_EVENT_PATH) + MAX_INT_STRING_SIZE];
};

const struct sensors_input_cache_entry_t *sensors_input_cache_get(
							const char *name);

#endif
