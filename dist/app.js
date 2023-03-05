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

function render_layouts() {
    var layouts_path = "/api/layouts";

    var xhhtp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            var layout_div = document.getElementById("key_layouts");
            var layout_json = JSON.parse(xhttp.responseText);
            var keymap = layout_json["layouts"];
            for (var layer in keymap) {
                // continue here
            }
        } else {
            _handle_server_error(xhttp);
        }
    }
}
