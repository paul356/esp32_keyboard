function clear_label() {
    document.getElementById("upload_resp").innerHTML = "";
}

function _handle_server_error(xhttp) {
    if (xhttp.status == 0) {
        alert("Server closed the connection abruptly!");
        location.reload();
    } else {
        alert(xhttp.status + " Error!\n" + xhttp.responseText);
        location.reload();
    }
}

function upload() {
    document.getElementById("upload_resp").innerHTML = "";

    var upload_path = "/upload/bin_file";
    var fileInput = document.getElementById("bin_file").files;

    if (fileInput.length == 0) {
        alert("No file selected!");
    } else {
        document.getElementById("bin_file").disabled = true;
        document.getElementById("upload").disabled = true;

        var file = fileInput[0];
        var xhttp = new XMLHttpRequest();
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

function _create_element(tag, content, attrs) {
    var div = document.createElement(tag);
    for (var attr in attrs) {
        div.setAttribute(attr, attrs[attr]);
    }
    if (content != null) {
        div.innerHTML = content;
    }
    return div;
}

function _create_div(content, attrs) {
    return _create_element("div", content, attrs);
}

function _render_keymap(layout_div, name, keymap) {
    layout_div.append(_create_div(name, {"class" : "title"}));
    
    var render_key = function(key, index, arr) {
        layout_div.append(_create_div(key, {"class" : "keystroke"}));
    }

    var render_row = function(row, index, arr) {
        // render row here
        row.forEach(render_key);
        layout_div.append(_create_element("br", null, {}));
    };
    keymap.forEach(render_row);
}

function render_layouts() {
    var layouts_path = "/api/layouts";

    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                var layout_div = document.getElementById("key_layouts");
                var layout_json = JSON.parse(xhttp.responseText);
                var keymap = layout_json["layouts"];
                for (var layer in keymap) {
                    _render_keymap(layout_div, layer, keymap[layer]);
                }
            } else {
                _handle_server_error(xhttp);
            }
        }
    }

    xhttp.open("GET", layouts_path, true);
    xhttp.send();
}

