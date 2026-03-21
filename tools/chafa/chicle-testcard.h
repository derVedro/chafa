/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2026 derVedro
 *
 * This file is part of Chafa, a program that shows pictures on text terminals.
 *
 * Chafa is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chafa is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Chafa.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef __CHICLE_TESTPAGE_H__
#define __CHICLE_TESTPAGE_H__

#include <glib.h>
#include <chafa.h>

G_BEGIN_DECLS

typedef struct
{
    guint8 *buffer;
    gint width;
    gint height;
    gint rowstride;
    ChafaPixelType pixel_type;
}
TestcardData;

TestcardData *chicle_test_card_generate(void);
void chicle_test_card_free(TestcardData *data);

G_END_DECLS

#endif /* __CHICLE_TESTPAGE_H__ */