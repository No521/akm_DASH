/******************************************************************************
 *
 * Copyright (C) 2015 Asahi Kasei Microdevices Corporation, Japan
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
 *
 ******************************************************************************/
#ifndef __AKL_DASH_EXT_H__
#define __AKL_DASH_EXT_H__

#include "AKL_APIs.h"

#define AKL_DRV_PATH_LEN  64

int AKL_DASH_Init(
    const uint8_t max_form
);

void AKL_DASH_Deinit(
    void
);

struct AKL_SCL_PRMS *AKL_DASH_Lock(
    void
);

void AKL_DASH_Unlock(
    void
);
#endif /* __AKL_DASH_EXT_H__ */
