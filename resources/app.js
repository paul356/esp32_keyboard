/*
 *  This program is a keyboard firmware for ESP family boards inspired by
 *  MK32 and qmk_firmware.
 *
 *  Copyright (C) 2024 github.com/paul356
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

var keymap_layouts = {};
var keymap_changed = {};
var basic_key_codes = null;
var quantum_funct_descs = null;
var mod_bit_names = null;
var macro_names = null;
var function_keys = null;
var max_layer_num = 0;
var selected_key = null;

function clear_label()
{
    document.getElementById("upload_resp").innerHTML = "";
}

function _handle_server_error(xhttp)
{
    if (xhttp.status == 0) {
        alert("Server closed the connection abruptly!");
        location.reload();
    } else {
        alert(xhttp.status + " Error!\n" + xhttp.responseText);
        location.reload();
    }
}

function upload_bin()
{
    document.getElementById("upload_resp").innerHTML = "";

    let upload_path = "/upload/bin_file";
    let fileInput = document.getElementById("bin_file").files;

    if (fileInput.length == 0) {
        alert("No file selected!");
    } else {
        document.getElementById("bin_file").disabled = true;
        document.getElementById("upload").disabled = true;
        document.getElementById("upload_resp").innerHTML = "<p style=\"color:Red\">Please wait... Don't unplug USB cable.</p>";

        let file = fileInput[0];
        let xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (xhttp.readyState == 4) {
                if (xhttp.status == 200) {
                    document.getElementById("upload_resp").innerHTML = xhttp.responseText;
                    document.getElementById("bin_file").disabled = false;
                    document.getElementById("upload").disabled = false;
                } else {
                    _handle_server_error(xhttp);
                }
            }
        };
        xhttp.open("POST", upload_path, true);
        xhttp.send(file);
    }
}

function _create_element(tag, content, attrs)
{
    let div = document.createElement(tag);
    for (let attr in attrs) {
        div.setAttribute(attr, attrs[attr]);
    }
    if (content != null) {
        div.innerHTML = content;
    }
    return div;
}

function _create_div(content, attrs)
{
    return _create_element("div", content, attrs);
}

function _funct_select(funct_name)
{
    var selected_index = -1;
    let funct_select = _create_element("select", null, {});

    let add_option = function(opt, index, arr) {
        if (opt["desc"] === funct_name) {
            selected_index = index;
        }
        let option = _create_element("option", opt["desc"], {});
        funct_select.append(option);
    }
    quantum_funct_descs.forEach(add_option);

    funct_select.selectedIndex = selected_index;

    return funct_select;
}

function _generic_code_select(generic_code, code_arr)
{
    var selected_index = -1;
    let gc_select = _create_element("select", null, {});

    let add_option = function(opt, index, arr) {
        if (opt === generic_code) {
            selected_index = index;
        }
        let option = _create_element("option", opt, {});
        gc_select.append(option);
    }
    code_arr.forEach(add_option);

    gc_select.selectedIndex = selected_index;

    return gc_select;
}

function _layer_select(layer_idx)
{
    var selected_index = -1;
    let layer_select = _create_element("select", null, {});

    let add_option = function(opt, index, arr) {
        if (opt.toString() === layer_idx) {
            selected_index = index;
        }
        let option = _create_element("option", opt.toString(), {});
        layer_select.append(option);
    }
    let layers = [...Array(max_layer_num).keys()];
    layers.forEach(add_option);

    layer_select.selectedIndex = selected_index;

    return layer_select;
}

function _mod_bit_select(mod_bits)
{
    let mod_bit_arr = mod_bits.split("|");
    let selected_index = -1;

    let mod_bit_select = _create_element("select", null, {"multiple" : ""});

    let add_option = function(opt, index, arr) {
        if (mod_bit_arr.includes(opt)) {
            let option = _create_element("option", opt, {"selected" : ""});
            mod_bit_select.append(option);
        } else {
            let option = _create_element("option", opt, {});
            mod_bit_select.append(option);
        }
    }
    mod_bit_names.forEach(add_option);

    return mod_bit_select;
}

function _get_funct_desc(funct_name)
{
    return quantum_funct_descs.find(element => element["desc"] === funct_name);
}

function _save_result(edit_div, disp_div)
{
    var full_key = Array(edit_div.children.length);
    for (var i = 0; i < edit_div.children.length; i++) {
        let opts = edit_div.children.item(i).selectedOptions;
        var argument_value = Array(opts.length);
        for (var j = 0; j < opts.length; j++) {
            argument_value[j] = opts.item(j).label;
        }
        full_key[i] = argument_value.join("|");
    }
    let full_key_str = full_key.join(" ");
    let layer_name = disp_div.getAttribute("layer-name");
    let row = parseInt(disp_div.getAttribute("row"));
    let col = parseInt(disp_div.getAttribute("col"));
    if (full_key_str !== keymap_layouts[layer_name][row][col]) {
        keymap_layouts[layer_name][row][col] = full_key_str;
        keymap_changed[layer_name][row][col] = true;
        disp_div.innerHTML = _get_display_key_name(full_key_str);
    }
}

function _keystroke_clicked(event)
{
    let target_div = event.currentTarget;
    let layer_name = target_div.getAttribute("layer-name");
    let row = parseInt(target_div.getAttribute("row"));
    let col = parseInt(target_div.getAttribute("col"));

    let full_key = keymap_layouts[layer_name][row][col];
    let toks = full_key.split(" ");

    let new_div = _create_div(null, {"class" : "keystroke", "layer-name" : layer_name, "row" : row, "col": col});

    let funct_select = _funct_select(toks[0]);
    new_div.append(funct_select);

    let funct_desc = _get_funct_desc(toks[0]);

    let arg_handler = function(opt, index, arr) {
        switch (opt) {
        case "layer_num":
            new_div.append(_layer_select(toks[index + 1]));
            break;
        case "basic_code":
            new_div.append(_generic_code_select(toks[index + 1], basic_key_codes));
            break;
        case "mod_bits":
            new_div.append(_mod_bit_select(toks[index + 1]));
            break;
        case "macro_code":
            new_div.append(_generic_code_select(toks[index + 1], macro_names));
            break;
        case "function_key_code":
            new_div.append(_generic_code_select(toks[index + 1], function_keys));
            break;
        }
    }
    funct_desc["arg_types"].forEach(arg_handler);

    funct_select.addEventListener("change", event => {
        let new_funct_name = event.currentTarget.selectedOptions.item(0).label;

        let curr_full_key = keymap_layouts[layer_name][row][col];

        for (var i = 0; i < new_div.children.length; i++) {
            let child = new_div.children.item(i);
            if (child !== funct_select) {
                child.remove();
            }
        }

        let funct_desc = _get_funct_desc(new_funct_name);
        funct_desc["arg_types"].forEach((opt, index, arr) => {
            switch (opt) {
            case "layer_num":
                new_div.append(_layer_select("0"));
                break;
            case "basic_code":
                new_div.append(_generic_code_select(basic_key_codes[0], basic_key_codes));
                break;
            case "mod_bits":
                new_div.append(_mod_bit_select(mod_bit_names[0]));
                break;
            case "macro_code":
                new_div.append(_generic_code_select(macro_names[0], macro_names));
                break;
            case "function_key_code":
                new_div.append(_generic_code_select(function_keys[0], function_keys));
                break;
            }
        });
    });

    target_div.replaceWith(new_div);
    if (selected_key !== null) {
        _save_result(selected_key[1], selected_key[0]);
        selected_key[1].replaceWith(selected_key[0]);
    }
    selected_key = [target_div, new_div];
    funct_select.focus();
}

function _get_display_key_name(full_key)
{
    let toks = full_key.split(' ')
    if (toks[0] === 'BS' || toks[0] === 'MA') {
        return toks[1];
    } else {
        return toks[0] + "(" + toks.slice(1).join(", ") + ")";
    }
}

function _render_keymap(layout_div, name, keymap)
{
    keymap_layouts[name] = keymap;
    keymap_changed[name] = new Array(keymap.length);
    for (let idx = 0; idx < keymap.length; idx += 1) {
        keymap_changed[name][idx] = new Array(keymap[idx].length);
        keymap_changed[name][idx].fill(false);
    }

    layout_div.append(_create_div(name, {"class" : "title"}));

    let render_row = function(row, i, rows) {
        // render row here
        let render_key = function(key, j, row) {
            let dis_str = _get_display_key_name(key);
            let key_div = _create_div(dis_str, {"class" : "keystroke", "layer-name" : name, "row" : i, "col" : j});
            key_div.addEventListener("click", _keystroke_clicked);
            layout_div.append(key_div);
        }

        row.forEach(render_key);
        layout_div.append(_create_element("br", null, {}));
    }
    keymap.forEach(render_row);
}

function _get_button_str(state)
{
    return state ? "Turn Off" : "Turn On";
}

function _render_status_line(status_json)
{
    let status_div = document.getElementById("status_line");

    for (let item in status_json) {
        status_div.append(_create_div(item + ":" + status_json[item], {"class" : "status-item"}));
    }
}

function _button_clicked(event)
{
    let button = event.currentTarget;
    let action_path_prefix = "/api/switches/";

    let state = button.getAttribute("curr_state");
    let funct = button.getAttribute("kb_function");

    button.disabled = true;

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                button.innerHTML = _get_button_str(state == "false") + " " + funct;
                button.setAttribute("curr_state", state == "false");
                button.disabled = false;
            } else {
                _handle_server_error(xhttp);
            }
        }
    };

    xhttp.open("PUT", action_path_prefix + funct, true);

    xhttp.setRequestHeader("Content-Type", "application/json");
    let data = JSON.stringify({"enabled" : state == "false"});
    xhttp.send(data);
}

function _update_wifi(event)
{
    let action_path = "/api/switches/WiFi"
    let button = event.currentTarget

    let wifi_select = document.getElementById("wifi_mode")
    let ssid   = document.getElementById("wifi_ssid").value
    let passwd = document.getElementById("wifi_password").value

    let mode = wifi_select.selectedOptions.item(0).label
    if (ssid.length == 0) {
        alert("ssid can't be empty")
        return
    }

    button.disabled = true

    let xhttp = new XMLHttpRequest()
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                button.disabled = false;
            } else {
                _handle_server_error(xhttp);
            }
        }
    }

    xhttp.open("PUT", action_path, true)

    xhttp.setRequestHeader("Content-Type", "application/json")
    let data = JSON.stringify({"mode" : mode, "ssid" : ssid, "passwd" : passwd})
    xhttp.send(data)
}

function _render_switches_line(status_json)
{
    let status_div = document.getElementById("status_line")

    let wifi_div = _create_element("div", null, {})
    let wifi_select = _create_element("select", null, {"id" : "wifi_mode"})
    let modes = Array.of("client", "hotspot")
    modes.forEach((element, idx, arr) => {
        wifi_select.append(_create_element("option", element, {}))
        if (element == status_json["wifi_state"]) {
            wifi_select.selectedIndex = idx
        }
    })
    wifi_div.append(_create_element("label", "WiFi Mode:", {}))
    wifi_div.append(wifi_select)
    wifi_div.append(_create_element("label", "SSID:", {}))
    wifi_div.append(_create_element("input", null, {"type" : "text", "id" : "wifi_ssid", "size" : "12"}))
    wifi_div.append(_create_element("label", "Password:", {}))
    wifi_div.append(_create_element("input", null, {"type" : "text", "id" : "wifi_password", "size" : "12"}))
    let wifi_btn = _create_element("button", "Change WiFi Mode", {})
    wifi_btn.addEventListener("click", _update_wifi)
    wifi_div.append(wifi_btn)

    status_div.append(wifi_div)

    let switch_div = _create_element("div", null, {})

    let ble_switch = _create_element("button", _get_button_str(status_json["ble_state"]) + " BLE", {"curr_state" : status_json["ble_state"], "kb_function" : "BLE"})
    ble_switch.addEventListener("click", _button_clicked)
    switch_div.append(ble_switch)

    let usb_switch = _create_element("button", _get_button_str(status_json["usb_state"]) + " USB", {"curr_state" : status_json["usb_state"], "kb_function" : "USB"})
    usb_switch.addEventListener("click", _button_clicked)
    switch_div.append(usb_switch)

    status_div.append(switch_div)
}

function render_status_line()
{
    let status_path = "/api/device-status";

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                let status_json = JSON.parse(xhttp.responseText);
                _render_status_line(status_json);
                _render_switches_line(status_json);
            } else {
                _handle_server_error(xhttp);
            }
        }  
    }

    xhttp.open("GET", status_path, true);
    xhttp.send();
}

function _get_keycodes(callback)
{
    let keycode_path = "/api/keycodes";

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                let keycode_json = JSON.parse(xhttp.responseText);
                basic_key_codes = keycode_json["keycodes"];
                quantum_funct_descs = keycode_json["quantum_functs"];
                mod_bit_names = keycode_json["mod_bits"];
                macro_names = keycode_json["macros"];
                function_keys = keycode_json["function_keys"];
                max_layer_num = keycode_json["layer_num"];

                callback();
            } else {
                _handle_server_error(xhttp);
            }
        }
    }

    xhttp.open("GET", keycode_path, true);
    xhttp.send();    
}

function _render_update_button()
{
    let layout_div = document.getElementById("key_layouts");

    let button_div = _create_div(null, {});

    let update_btn = _create_element("button", "Update", {"id" : "button_ok"});
    update_btn.addEventListener('click', event => update_keymap());
    let reset_btn = _create_element("button", "Restore", {"id" : "button_reset"});
    reset_btn.addEventListener('click', event => restore_default_keymap());

    button_div.append(update_btn);
    button_div.append(reset_btn);

    layout_div.append(button_div);
}

function _render_layouts()
{
    let layouts_path = "/api/layouts";

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                let layout_div = document.getElementById("key_layouts");
                let layout_json = JSON.parse(xhttp.responseText);
                let keymap = layout_json["layouts"];
                for (let layer in keymap) {
                    _render_keymap(layout_div, layer, keymap[layer]);
                }
                _render_update_button();
            } else {
                _handle_server_error(xhttp);
            }
        }
    }

    xhttp.open("GET", layouts_path, true);
    xhttp.send();
}

function _render_macros()
{
    let render_macro = function(macro_name) {
        let macro_div = _create_div(null, {"class" : "macro_div"});
        let label   = _create_element("label", macro_name+":", {"class" : "macro_label"});
        let content = _create_element("textarea", null, {"class" : "macro_content"});
        let btn = _create_element("button", "Update " + macro_name, {"class" : "macro_button"});

        macro_div.append(label);
        macro_div.append(content);
        macro_div.append(btn);

        btn.addEventListener("click", event => {
            let request_path = "api/macro/" + macro_name;
            let xhttp = new XMLHttpRequest();
            btn.disabled = true;
            xhttp.onreadystatechange = function() {
                if (xhttp.readyState == 4) {
                    if (xhttp.status == 200) {
                        btn.disabled = false;
                    } else {
                        _handle_server_error(xhttp);
                    }
                }
            }

            xhttp.open("PUT", request_path, true);
            let json = {};
            json[macro_name] = content.value;
            let data = JSON.stringify(json);
            xhttp.send(data);
        });

        let request_path = "/api/macro/" + macro_name;
        let xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (xhttp.readyState == 4) {
                if (xhttp.status == 200) {
                    let ret_json = JSON.parse(xhttp.responseText);
                    content.value = ret_json[macro_name];
                } else {
                    content.value = "fail to get this macro";
                }
            }
        }

        xhttp.open("GET", request_path, true);
        xhttp.send();

        return macro_div;
    }

    let macros_div = document.getElementById("macros");
    macro_names.forEach(macro_name => macros_div.append(render_macro(macro_name)));
}

function render_layouts_and_macros()
{
    let callback = function() {
        _render_layouts();
        _render_macros();
    }
    
    _get_keycodes(callback);
}

function _update_keymap_layer(layer_name)
{
    let update_arr = [];
    let keycodes = [];
    let col_size = keymap_changed[layer_name][0].length;

    let visit_row = function(row, i, arr) {
        let visit_col = function(changed, j, arr) {
            if (changed) {
                update_arr.push(i * col_size + j);
                keycodes.push(keymap_layouts[layer_name][i][j]);
            }
        }
        row.forEach(visit_col);
    }

    keymap_changed[layer_name].forEach(visit_row);

    if (update_arr.length == 0) {
        return;
    }

    let update_path = "/api/keymap";

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                document.getElementById("button_ok").disabled = false;
            } else {
                _handle_server_error(xhttp);
            }
        }
    }

    document.getElementById("button_ok").disabled = true;

    xhttp.open("PUT", update_path, true);
    xhttp.setRequestHeader("Content-Type", "application/json");
    let data = JSON.stringify({"layer" : layer_name, "positions" : update_arr, "keycodes" : keycodes});
    xhttp.send(data);
}

function update_keymap()
{
    for (let layer_name in keymap_changed) {
        _update_keymap_layer(layer_name);
    }
}

function restore_default_keymap()
{
    let reset_path = "/api/keymap";

    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                location.reload();
            } else {
                _handle_server_error(xhttp);
            }
        }
    }

    document.getElementById("button_reset").disabled = true;

    xhttp.open("POST", reset_path, true);
    xhttp.setRequestHeader("Content-Type", "application/json");
    let data = JSON.stringify({"positions" : [], "keycodes" : []});
    xhttp.send(data);
}

function _is_child_of(elem, parent)
{
    let children = parent.children;
    for (var i = 0; i < children.length; i++) {
        if (elem === children.item(i) || _is_child_of(elem, children.item(i))) {
            return true;
        }
    }

    return false;
}

function handle_body_clicks()
{
    // Body can recieve all bubbled clicks from its children and grandchildren.
    // We will restore selected key back to a div if the user clicks on other keys or elements.
    document.body.addEventListener("click", event => {
        if (selected_key !== null &&
            !_is_child_of(event.target, selected_key[1]) &&
            event.target !== selected_key[0] &&
            event.target !== selected_key[1]) {
            _save_result(selected_key[1], selected_key[0]);
            selected_key[1].replaceWith(selected_key[0]);
            selected_key = null;
        }
    });
}

function render_tabs(evt, tab_name) {
  // Declare all variables
  var i, tabcontent, tablinks;

  // Get all elements with class="tabcontent" and hide them
  tabcontent = document.getElementsByClassName("tabcontent");
  for (i = 0; i < tabcontent.length; i++) {
    tabcontent[i].style.display = "none";
  }

  // Get all elements with class="tablinks" and remove the class "active"
  tablinks = document.getElementsByClassName("tablinks");
  for (i = 0; i < tablinks.length; i++) {
    tablinks[i].className = tablinks[i].className.replace(" active", "");
  }

  // Show the current tab, and add an "active" class to the button that opened the tab
  document.getElementById(tab_name).style.display = "block";
  evt.currentTarget.className += " active";
}

