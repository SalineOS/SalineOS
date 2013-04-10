/* Copyright (C) 2012 André Stösel <andre@stoesel.de>
   This file is licensed under the terms of the expat license, see the file EXPAT. */

[CCode (cprefix = "Katze", lower_case_cprefix = "katze_")]
namespace Katze {
    static void assert_str_equal (string input, string result, string? expected);
    static unowned Gtk.Widget property_proxy (void* object, string property, string? hint);

    [CCode (cheader_filename = "katze/katze.h")]
    public class Array : Katze.Item {
        public Array (GLib.Type type);
        public void add_item (GLib.Object item);
        public uint get_length ();
        public GLib.List<unowned Item> get_items ();
    }

    [CCode (cheader_filename = "katze/katze.h")]
    public class Item : GLib.Object {
        public string? uri { get; set; }
        public string? name { get; set; }
        public string? text { get; set; }

        public bool get_meta_boolean (string key);
        public int64 get_meta_integer (string key);
        public void set_meta_integer (string key, int64 value);
    }
}

