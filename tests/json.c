/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/datetime.h"

static void test_json_encode(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	{
		JsonNode *json = json_decode("{\"a\"\n0}");
		CuAssertPtrEquals(tc, NULL, json);
	}
	{
		JsonNode *json = json_decode("\"key 1\": 1234}");
		CuAssertPtrEquals(tc, NULL, json);
	}
	{
		JsonNode *json = json_decode("{\"key 1\": 1234");
		CuAssertPtrEquals(tc, NULL, json);
	}
	{
		JsonNode *json = json_decode("{\"key 1\": 1234}}");
		CuAssertPtrEquals(tc, NULL, json);
	}
	{
		JsonNode *json = json_decode("\"key 1\"}: 1234");
		CuAssertPtrEquals(tc, NULL, json);
	}
	{
		JsonNode *json = json_decode("\"key {1\": 1234");
		CuAssertPtrEquals(tc, NULL, json);
	}
	{
		JsonNode *json = json_decode("{\"a\", 0}");
		CuAssertPtrEquals(tc, NULL, json);
	}
	{
		JsonNode *json = json_decode("{\"a\", {2}}");
		CuAssertPtrEquals(tc, NULL, json);
	}
	{
		JsonNode *json = json_decode("{\"a\", {2: 3}}");
		CuAssertPtrEquals(tc, NULL, json);
	}
	{
		JsonNode *json = json_decode("{\"a\": 1]");
		CuAssertPtrEquals(tc, NULL, json);
	}
	{
		JsonNode *json = json_decode("{a: 1]");
		CuAssertPtrEquals(tc, NULL, json);
	}
	{
		JsonNode *json = json_decode("{\"a\", {\"a\": 2 3}}");
		CuAssertPtrEquals(tc, NULL, json);
	}

	/*
	 * Read valid
	 */
	{
		JsonNode *json = json_decode("{}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("[]");
		CuAssertIntEquals(tc, JSON_ARRAY, json->tag);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("[{},{}]");
		CuAssertIntEquals(tc, JSON_ARRAY, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertIntEquals(tc, JSON_OBJECT, json->children.head->tag);
		CuAssertIntEquals(tc, JSON_OBJECT, json->children.head->next->tag);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("[{},[]]");
		CuAssertIntEquals(tc, JSON_ARRAY, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertIntEquals(tc, JSON_OBJECT, json->children.head->tag);
		CuAssertIntEquals(tc, JSON_ARRAY, json->children.head->next->tag);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("[10]");
		CuAssertIntEquals(tc, JSON_ARRAY, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertIntEquals(tc, JSON_NUMBER, json->children.head->tag);
		CuAssertIntEquals(tc, 10, json->children.head->number_);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"a\": 0}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertIntEquals(tc, JSON_NUMBER, json->children.head->tag);
		CuAssertIntEquals(tc, 0, json->children.head->number_);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"a\": []}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertIntEquals(tc, 'a', json->children.head->key[0]);
		CuAssertIntEquals(tc, JSON_ARRAY, json->children.head->tag);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"boolVar\": true}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertStrEquals(tc, "boolVar", json->children.head->key);
		CuAssertIntEquals(tc, JSON_BOOL, json->children.head->tag);
		CuAssertIntEquals(tc, true, json->children.head->bool_);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"boolVar\": false}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertStrEquals(tc, "boolVar", json->children.head->key);
		CuAssertIntEquals(tc, JSON_BOOL, json->children.head->tag);
		CuAssertIntEquals(tc, false, json->children.head->bool_);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"nullVar\": null}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertStrEquals(tc, "nullVar", json->children.head->key);
		CuAssertIntEquals(tc, JSON_NULL, json->children.head->tag);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"intVar\": 12}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertStrEquals(tc, "intVar", json->children.head->key);
		CuAssertIntEquals(tc, JSON_NUMBER, json->children.head->tag);
		CuAssertIntEquals(tc, 12, json->children.head->number_);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"floatVar\": 12.345}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertStrEquals(tc, "floatVar", json->children.head->key);
		CuAssertIntEquals(tc, JSON_NUMBER, json->children.head->tag);
		CuAssertDblEquals(tc, 12.345, json->children.head->number_, 0);
		CuAssertIntEquals(tc, 3, json->children.head->decimals_);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"strVar\": \"hello world\"}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertStrEquals(tc, "strVar", json->children.head->key);
		CuAssertIntEquals(tc, JSON_STRING, json->children.head->tag);
		CuAssertStrEquals(tc, "hello world", json->children.head->string_);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"strVar\" : \"escapes: \\/\\r\\n\\t\\b\\f\\\"\\\\\"}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertStrEquals(tc, "strVar", json->children.head->key);
		CuAssertIntEquals(tc, JSON_STRING, json->children.head->tag);
		CuAssertStrEquals(tc, "escapes: /\r\n\t\b\f\"\\", json->children.head->string_);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"strVar\" : \"\"}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertStrEquals(tc, "strVar", json->children.head->key);
		CuAssertIntEquals(tc, JSON_STRING, json->children.head->tag);
		CuAssertIntEquals(tc, 0, strlen(json->children.head->string_));
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"strVar\" : \"AbcD\"}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertStrEquals(tc, "strVar", json->children.head->key);
		CuAssertIntEquals(tc, JSON_STRING, json->children.head->tag);
		CuAssertStrEquals(tc, "AbcD", json->children.head->string_);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"a\":{},\"b\":{}}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertIntEquals(tc, JSON_OBJECT, json->children.head->tag);
		CuAssertIntEquals(tc, 'a', json->children.head->key[0]);
		CuAssertPtrNotNull(tc, json->children.head->next);
		CuAssertIntEquals(tc, JSON_OBJECT, json->children.head->next->tag);
		CuAssertIntEquals(tc, 'b', json->children.head->next->key[0]);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("[ 1, true, [123, \"hello\"]]");
		CuAssertIntEquals(tc, JSON_ARRAY, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertIntEquals(tc, JSON_NUMBER, json->children.head->tag);
		CuAssertIntEquals(tc, 1, json->children.head->number_);
		CuAssertPtrNotNull(tc, json->children.head->next);
		CuAssertIntEquals(tc, JSON_BOOL, json->children.head->next->tag);
		CuAssertIntEquals(tc, true, json->children.head->next->bool_);
		CuAssertPtrNotNull(tc, json->children.head->next->next);
		CuAssertIntEquals(tc, JSON_ARRAY, json->children.head->next->next->tag);
		CuAssertPtrNotNull(tc, json->children.head->next->next->children.head);
		CuAssertIntEquals(tc, JSON_NUMBER, json->children.head->next->next->children.head->tag);
		CuAssertIntEquals(tc, 123, json->children.head->next->next->children.head->number_);
		CuAssertPtrNotNull(tc, json->children.head->next->next->children.head->next);
		CuAssertIntEquals(tc, JSON_STRING, json->children.head->next->next->children.head->next->tag);
		CuAssertStrEquals(tc, "hello", json->children.head->next->next->children.head->next->string_);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\"a\": 0, \"b\": \"c\"}");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertIntEquals(tc, JSON_NUMBER, json->children.head->tag);
		CuAssertIntEquals(tc, 'a', json->children.head->key[0]);
		CuAssertIntEquals(tc, 0, json->children.head->number_);
		CuAssertPtrNotNull(tc, json->children.head->next);
		CuAssertIntEquals(tc, JSON_STRING, json->children.head->next->tag);
		CuAssertIntEquals(tc, 'b', json->children.head->next->key[0]);
		CuAssertIntEquals(tc, 'c', json->children.head->next->string_[0]);
		json_delete(json);
	}
	{
		JsonNode *json = json_decode("{\n \"Day\": 26,\n \"Month\": 9,\n \"Year\": 12\n }");
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);
		CuAssertPtrNotNull(tc, json->children.head);
		CuAssertIntEquals(tc, JSON_NUMBER, json->children.head->tag);
		CuAssertStrEquals(tc, "Day", json->children.head->key);
		CuAssertIntEquals(tc, 26, json->children.head->number_);
		CuAssertPtrNotNull(tc, json->children.head->next);
		CuAssertIntEquals(tc, JSON_NUMBER, json->children.head->next->tag);
		CuAssertStrEquals(tc, "Month", json->children.head->next->key);
		CuAssertIntEquals(tc, 9, json->children.head->next->number_);
		CuAssertPtrNotNull(tc, json->children.head->next->next);
		CuAssertIntEquals(tc, JSON_NUMBER, json->children.head->next->next->tag);
		CuAssertStrEquals(tc, "Year", json->children.head->next->next->key);
		CuAssertIntEquals(tc, 12, json->children.head->next->next->number_);
		CuAssertPtrEquals(tc, NULL, json->children.head->next->next->next);
		json_delete(json);
	}
	{
		char *in = "{\"height\":10,\"layers\":[{\"data\":[6,6],\"height\":10,"
		"\"name\":\"Calque de Tile 1\",\"opacity\":1,\"type\":\"tilelayer\","
		"\"visible\":true,\"width\":10,\"x\":0,\"y\":0}],"
		"\"orientation\":\"orthogonal\",\"properties\":{},\"tileheight\":32,"
		"\"tilesets\":[{\"firstgid\":1,\"image\":\"../images/tiles.png\","
		"\"imageheight\":64,\"imagewidth\":160,\"margin\":0,\"name\":\"Tiles\","
		"\"properties\":{},\"spacing\":0,\"tileheight\":32,\"tilewidth\":32}],"
		"\"tilewidth\":32,\"version\":1,\"width\":10}";
		assert(json_validate(in) == true);
		JsonNode *json = json_decode(in);
		CuAssertIntEquals(tc, JSON_OBJECT, json->tag);

		double nr = 0;
		CuAssertIntEquals(tc, 0, json_find_number(json, "height", &nr));
		CuAssertIntEquals(tc, 10, nr);

		char *str = NULL;
		CuAssertIntEquals(tc, 0, json_find_string(json, "orientation", &str));
		CuAssertStrEquals(tc, "orthogonal", str);
		
		JsonNode *layers = json_find_member(json, "layers");
		CuAssertPtrNotNull(tc, layers);
		CuAssertIntEquals(tc, JSON_ARRAY, layers->tag);
		CuAssertStrEquals(tc, "layers", layers->key);

		JsonNode *obj = json_find_element(layers, 0);
		CuAssertPtrNotNull(tc, obj);
		CuAssertIntEquals(tc, JSON_OBJECT, obj->tag);

		JsonNode *data = json_find_member(obj, "data");
		CuAssertPtrNotNull(tc, data);
		CuAssertIntEquals(tc, JSON_ARRAY, data->tag);
		CuAssertStrEquals(tc, "data", data->key);

		JsonNode *el = json_find_element(data, 0);
		CuAssertPtrNotNull(tc, el);
		CuAssertIntEquals(tc, JSON_NUMBER, el->tag);
		CuAssertIntEquals(tc, 6, el->number_);

		el = json_first_child(data);
		CuAssertPtrNotNull(tc, el);
		CuAssertIntEquals(tc, JSON_NUMBER, el->tag);
		CuAssertIntEquals(tc, 6, el->number_);

		str = json_stringify(json, NULL);
		CuAssertStrEquals(tc, in, str);
		json_free(str);

		JsonNode *clone = NULL;
		CuAssertIntEquals(tc, 0, json_clone(json, &clone));
	
		str = json_stringify(clone, NULL);
		CuAssertStrEquals(tc, in, str);
		json_free(str);

		json_delete(clone);
		json_delete(json);
	}

	/*
	 * Not critical functions
	 */
	{
		JsonNode *json = json_mkobject();
		char *str = json_encode(json);
		CuAssertStrEquals(tc, "{}", str);
		json_delete(json);
		json_free(str);
	}
	{
		char *str = json_encode_string("hello world");
		CuAssertStrEquals(tc, "\"hello world\"", str);
		json_free(str);
	}

	CuAssertIntEquals(tc, 0, xfree());
}

static void test_json_decode(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	
	/*
	 * Read valid
	 */
	{
		JsonNode *json = json_mkobject();
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkarray();
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "[]", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkarray();
		json_append_element(json, json_mkobject());
		json_append_element(json, json_mkobject());
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, out, "[{},{}]");
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkarray();
		json_append_element(json, json_mknumber(10, 0));
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, out, "[10]");
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "a", json_mknumber(0, 0));
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, out, "{\"a\":0}");
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "a", json_mkarray());
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"a\":[]}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "boolVar", json_mkbool(true));
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"boolVar\":true}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "boolVar", json_mkbool(false));
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"boolVar\":false}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "nullVar", json_mknull());
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"nullVar\":null}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "intVar", json_mknumber(12, 0));
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"intVar\":12}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "floatVar", json_mknumber(12.345, 3));
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"floatVar\":12.345}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "strVar", json_mkstring("hello world"));
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"strVar\":\"hello world\"}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "strVar", json_mkstring("escapes: /\r\n\t\b\f\"\\"));
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"strVar\":\"escapes: /\\r\\n\\t\\b\\f\\\"\\\\\"}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "strVar", json_mkstring(""));
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"strVar\":\"\"}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "strVar", json_mkstring("AbcD"));
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"strVar\":\"AbcD\"}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "a", json_mkobject());
		json_append_member(json, "b", json_mkobject());
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"a\":{},\"b\":{}}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkarray();
		json_append_element(json, json_mknumber(1, 0));
		json_append_element(json, json_mkbool(true));
		JsonNode *array = json_mkarray();
		json_append_element(array, json_mknumber(123, 0));
		json_append_element(array, json_mkstring("hello"));
		json_append_element(json, array);
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "[1,true,[123,\"hello\"]]", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "a", json_mknumber(0, 0));
		json_append_member(json, "b", json_mkstring("c"));
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"a\":0,\"b\":\"c\"}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "Day", json_mknumber(26, 0));
		json_append_member(json, "Month", json_mknumber(9, 0));
		json_append_member(json, "Year", json_mknumber(12, 0));
		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"Day\":26,\"Month\":9,\"Year\":12}", out);
		json_free(out);
		json_delete(json);
	}
	{
		char *in = "{\"height\":10,\"layers\":[{\"data\":[6,6],\"height\":10,"
		"\"name\":\"Calque de Tile 1\",\"opacity\":1,\"type\":\"tilelayer\","
		"\"visible\":true,\"width\":10,\"x\":0,\"y\":0}],"
		"\"orientation\":\"orthogonal\",\"properties\":{},\"tileheight\":32,"
		"\"tilesets\":[{\"firstgid\":1,\"image\":\"../images/tiles.png\","
		"\"imageheight\":64,\"imagewidth\":160,\"margin\":0,\"name\":\"Tiles\","
		"\"properties\":{},\"spacing\":0,\"tileheight\":32,\"tilewidth\":32}],"
		"\"tilewidth\":32,\"version\":1,\"width\":10}";


		JsonNode *json = json_mkobject();
		json_append_member(json, "height", json_mknumber(10, 0));

		JsonNode *layers = json_mkarray();
		JsonNode *layers1 = json_mkobject();

		JsonNode *data = json_mkarray();
		json_append_element(data, json_mknumber(6, 0));
		json_append_element(data, json_mknumber(6, 0));
		json_append_member(layers1, "data", data);

		json_append_member(layers1, "height", json_mknumber(10, 0));
		json_append_member(layers1, "name", json_mkstring("Calque de Tile 1"));
		json_append_member(layers1, "opacity", json_mknumber(1, 0));
		json_append_member(layers1, "type", json_mkstring("tilelayer"));
		json_append_member(layers1, "visible", json_mkbool(true));
		json_append_member(layers1, "width", json_mknumber(10, 0));
		json_append_member(layers1, "x", json_mknumber(0, 0));
		json_append_member(layers1, "y", json_mknumber(0, 0));
		json_append_element(layers, layers1);

		json_append_member(json, "layers", layers);

		json_append_member(json, "orientation", json_mkstring("orthogonal"));
		json_append_member(json, "properties", json_mkobject());
		json_append_member(json, "tileheight", json_mknumber(32, 0));

		JsonNode *tilesets = json_mkarray();
		JsonNode *tilesets1 = json_mkobject();

		json_append_member(tilesets1, "firstgid", json_mknumber(1, 0));
		json_append_member(tilesets1, "image", json_mkstring("../images/tiles.png"));
		json_append_member(tilesets1, "imageheight", json_mknumber(64, 0));
		json_append_member(tilesets1, "imagewidth", json_mknumber(160, 0));
		json_append_member(tilesets1, "margin", json_mknumber(0, 0));
		json_append_member(tilesets1, "name", json_mkstring("Tiles"));
		json_append_member(tilesets1, "properties", json_mkobject());
		json_append_member(tilesets1, "spacing", json_mknumber(0, 0));
		json_append_member(tilesets1, "tileheight", json_mknumber(32, 0));
		json_append_member(tilesets1, "tilewidth", json_mknumber(32, 0));
		json_append_element(tilesets, tilesets1);

		json_append_member(json, "tilesets", tilesets);
		json_append_member(json, "tilewidth", json_mknumber(32, 0));
		json_append_member(json, "version", json_mknumber(1, 0));
		json_append_member(json, "width", json_mknumber(10, 0));

		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, in, out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "a", json_mknumber(0, 0));
		json_prepend_member(json, "b", json_mknumber(1, 0));

		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"b\":1,\"a\":0}", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkarray();
		json_append_element(json, json_mknumber(0, 0));
		json_prepend_element(json, json_mknumber(1, 0));

		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "[1,0]", out);
		json_free(out);
		json_delete(json);
	}
	{
		JsonNode *json = json_mkobject();
		json_append_member(json, "a", json_mknumber(0, 0));
		json_prepend_member(json, "b", json_mknumber(1, 0));

		JsonNode *child = json_find_member(json, "a");
		assert(child != NULL);
		json_remove_from_parent(child);
		json_delete(child);

		char *out = json_stringify(json, NULL);
		CuAssertStrEquals(tc, "{\"b\":1}", out);
		json_free(out);
		json_delete(json);
	}

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_json(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_json_encode);
	SUITE_ADD_TEST(suite, test_json_decode);

	return suite;
}