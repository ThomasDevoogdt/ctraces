/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  CTraces
 *  =======
 *  Copyright 2022 Eduardo Silva <eduardo@calyptia.com>
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

#include <ctraces/ctraces.h>

#include <cfl/cfl.h>
#include <cfl/cfl_time.h>
#include <cfl/cfl_kvlist.h>

/* register a span to the ctrace context and sets the span id */
static void span_register(struct ctrace *ctx, struct ctrace_span *span)
{
    ctx->last_span_id++;
    span->id = ctx->last_span_id;
}

struct ctrace_span *ctr_span_create(struct ctrace *ctx, cfl_sds_t name,
                                    struct ctrace_span *parent)
{
    struct ctrace_span *span;

    if (!name) {
        return NULL;
    }

    span = calloc(1, sizeof(struct ctrace_span));
    if (!span) {
        ctr_errno();
        return NULL;
    }
    span->name = cfl_sds_create(name);
    if (!span->name) {
        free(span);
        return NULL;
    }

    span->attr = ctr_attributes_create();
    if (!span->attr) {
        free(span);
        return NULL;
    }

    cfl_list_init(&span->events);
    cfl_list_init(&span->childs);

    if (parent) {
        /* If a parent span was given, link to the 'childs' list */
        cfl_list_add(&span->_head, &parent->childs);
        span->parent_span_id = parent->id;
    }
    else {
        /* link directly to the ctrace context list of spans */
        cfl_list_add(&span->_head, &ctx->spans);
    }

    /* set default kind */
    ctr_span_kind_set(span, CTRACE_SPAN_INTERNAL);

    /* always start a span by default, the start can be overriden later if needed */
    ctr_span_start(ctx, span);

    span_register(ctx, span);
    return span;
}

int ctr_span_kind_set(struct ctrace_span *span, int kind)
{
    if (kind < CTRACE_SPAN_UNSPECIFIED || kind > CTRACE_SPAN_CONSUMER) {
        return -1;
    }

    span->kind = kind;
    return 0;
}

/* returns a read-only version of the Span kind */
char *ctr_span_kind_string(struct ctrace_span *span)
{
    switch (span->kind) {
        case CTRACE_SPAN_INTERNAL:
            return "internal";
        case CTRACE_SPAN_SERVER:
            return "server";
        case CTRACE_SPAN_CLIENT:
            return "client";
        case CTRACE_SPAN_PRODUCER:
            return "producer";
        case CTRACE_SPAN_CONSUMER:
            return "consumer";
        default:
            return "unspecified";
    };
}

/*
 * Span attributes
 * ---------------
 */
int ctr_span_set_attribute_string(struct ctrace_span *span, char *key, char *value)
{
    return ctr_attributes_set_string(span->attr, key, value);
}

int ctr_span_set_attribute_bool(struct ctrace_span *span, char *key, int b)
{
    return ctr_attributes_set_bool(span->attr, key, b);
}

int ctr_span_set_attribute_int(struct ctrace_span *span, char *key, int value)
{
    return ctr_attributes_set_int(span->attr, key, value);
}

int ctr_span_set_attribute_double(struct ctrace_span *span, char *key, double value)
{
    return ctr_attributes_set_double(span->attr, key, value);
}

int ctr_span_set_attribute_array(struct ctrace_span *span, char *key,
                                 struct cfl_array *value)
{
    return ctr_attributes_set_array(span->attr, key, value);
}

int ctr_span_set_attribute_kvlist(struct ctrace_span *span, char *key,
                                  struct cfl_kvlist *value)
{

    return ctr_attributes_set_kvlist(span->attr, key, value);
}

void ctr_span_start(struct ctrace *ctx, struct ctrace_span *span)
{
    span->start_time = cfl_time_now();
}

void ctr_span_end(struct ctrace *ctx, struct ctrace_span *span)
{
    span->end_time = cfl_time_now();
}

void ctr_span_destroy(struct ctrace_span *span)
{
    struct cfl_list *tmp;
    struct cfl_list *head;
    struct ctrace_span *child;
    struct ctrace_span_event *event;

    if (span->name) {
        cfl_sds_destroy(span->name);
    }

    if (span->attr) {
        ctr_attributes_destroy(span->attr);
    }

    /* events */
    cfl_list_foreach_safe(head, tmp, &span->events) {
        event = cfl_list_entry(head, struct ctrace_span_event, _head);
        ctr_span_event_delete(event);
    }

    /* childs */
    cfl_list_foreach_safe(head, tmp, &span->childs) {
        child = cfl_list_entry(head, struct ctrace_span, _head);
        ctr_span_destroy(child);
    }

    cfl_list_del(&span->_head);
    free(span);
}

/*
 * Span Events
 * -----------
 */
struct ctrace_span_event *ctr_span_event_add(struct ctrace_span *span, char *name)
{
    struct ctrace_span_event *ev;

    if (!name) {
        return NULL;
    }

    ev = calloc(1, sizeof(struct ctrace_span_event));
    if (!ev) {
        ctr_errno();
        return NULL;
    }
    ev->name = cfl_sds_create(name);
    if (!ev->name) {
        free(ev);
        return NULL;
    }

    return ev;
}

int ctr_span_event_set_attribute_string(struct ctrace_span_event *event, char *key, char *value)
{
    return ctr_attributes_set_string(event->attr, key, value);
}

int ctr_span_event_set_attribute_bool(struct ctrace_span_event *event, char *key, int b)
{
    return ctr_attributes_set_bool(event->attr, key, b);
}

int ctr_span_event_set_attribute_int(struct ctrace_span_event *event, char *key, int value)
{
    return ctr_attributes_set_int(event->attr, key, value);
}

int ctr_span_event_set_attribute_double(struct ctrace_span_event *event, char *key, double value)
{
    return ctr_attributes_set_double(event->attr, key, value);
}

int ctr_span_event_set_attribute_array(struct ctrace_span_event *event, char *key,
                                       struct cfl_array *value)
{
    return ctr_attributes_set_array(event->attr, key, value);
}

int ctr_span_event_set_attribute_kvlist(struct ctrace_span_event *event, char *key,
                                        struct cfl_kvlist *value)
{

    return ctr_attributes_set_kvlist(event->attr, key, value);
}

void ctr_span_event_delete(struct ctrace_span_event *event)
{
    if (event->name) {
        cfl_sds_destroy(event->name);
    }
    cfl_list_del(&event->_head);
    free(event);
}

