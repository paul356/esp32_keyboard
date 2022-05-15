import "./common.scss";
import "./style.scss";

const buttonOk = document.getElementById("button_ok");
const buttonReset = document.getElementById("button_reset");
const row1 = document.getElementById("row_1");
const row2 = document.getElementById("row_2");
const row3 = document.getElementById("row_3");
const row4 = document.getElementById("row_4");
const row5 = document.getElementById("row_5");
const row6 = document.getElementById("row_6");

// document.getElementById("app").addEventListener("click",function(event){
//   var target = event.target;
//   console.log(event);
// });

buttonOk.onclick = function (event) {
  console.log(event);
};

buttonReset.onclick = function (event) {
  console.log(event);
};

const elements1 = [{ item: "Esc", width: 50 }];
Array.from(Array(12).keys()).forEach((num) => {
  elements1.push({
    item: `F${num + 1}`,
    width: 50,
  });
});
elements1.push({
  item: "Delete",
  width: 80,
  marginLeft: 20,
});

elements1.push({
  item: "Home",
  width: 50,
  marginLeft: 40,
});

row1.innerHTML = elements1
  .map((item) => {
    return `<div class="key" style="width: ${item.width};${
      item.marginLeft ? "margin-left:" + item.marginLeft + "px" : ""
    }">
    <div class="inner">
      <div class="align-center pl1 lh40" data-val="${item.item}">${
      item.item
    }</div>
    </div>
  </div>`;
  })
  .join("");

const elements2 = [
  { top: "~", bottom: "`" },
  { top: "!", bottom: "1" },
  { top: "@", bottom: "2" },
  { top: "#", bottom: "3" },
  { top: "$", bottom: "4" },
  { top: "%", bottom: "5" },
  { top: "^", bottom: "6" },
  { top: "&", bottom: "7" },
  { top: "*", bottom: "8" },
  { top: "(", bottom: "9" },
  { top: ")", bottom: "0" },
  { top: "_", bottom: "-" },
  { top: "+", bottom: "=" },
  { name: "Backspace", width: 100 },
];

elements2.push({
  name: "End",
  width: 50,
  marginLeft: 40,
});

row2.innerHTML = elements2
  .map((item) => {
    if (item.top) {
      return `<div class="key">
      <div class="inner flex-column space-between">
        <div class="f12 pl1">${item.top}</div>
        <div class="f12 pl1">${item.bottom}</div>
      </div>
    </div>`;
    } else if (item.name === "End") {
      return `<div class="key" style="width: ${item.width};${
        item.marginLeft && "margin-left:" + item.marginLeft + "px"
      }">
      <div class="inner">
        <div class="align-center pl1 lh40">${item.name}</div>
      </div>
    </div>`;
    } else {
      return `<div class="key" style="width: ${item.width}">
      <div class="inner">
        <div class="${item.name === "Backspace" && "lh40"}">${item.name}</div>
      </div>
    </div>`;
    }
  })
  .join("");

const elements3 = [{ item: "Tab", width: 75 }];

["Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"].forEach((char) => {
  elements3.push({
    item: char,
    width: 50,
  });
});

elements3.push({
  item: ["{", "["],
  width: 50,
});

elements3.push({
  item: ["}", "]"],
  width: 50,
});

elements3.push({
  item: ["|", "\\"],
  width: 75,
});

elements3.push({
  item: "Page Up",
  width: 50,
  marginLeft: 40,
});

row3.innerHTML = elements3
  .map((item) => {
    if (Array.isArray(item.item)) {
      return `<div class="key" style="width: ${item.width}">
      <div class="inner flex-column space-between">
        <div class="f12 pl1">${item.item[0]}</div>
        <div class="f12 pl1">${item.item[1]}</div>
      </div>
    </div>`;
    } else {
      return `<div class="key" style="width: ${item.width};${
        item.marginLeft && "margin-left:" + item.marginLeft + "px"
      }">
      <div class="inner">
        <div>${item.item}</div>
      </div>
    </div>`;
    }
  })
  .join("");

const elements4 = [{ item: "Caps Lock", width: 100 }];

["A", "S", "D", "F", "G", "H", "J", "K", "L"].forEach((char) => {
  elements4.push({
    item: char,
    width: 50,
  });
});

elements4.push({
  item: [":", ";"],
  width: 50,
});

elements4.push({
  item: ['"', "'"],
  width: 50,
});

elements4.push({
  item: "Enter",
  width: 100,
});

elements4.push({
  item: "Page Down",
  width: 50,
  marginLeft: 40,
});

row4.innerHTML = elements4
  .map((item) => {
    if (Array.isArray(item.item)) {
      return `<div class="key" style="width: ${item.width}">
      <div class="inner flex-column space-between">
        <div class="f12 pl1">${item.item[0]}</div>
        <div class="f12 pl1">${item.item[1]}</div>
      </div>
    </div>`;
    } else {
      return `<div class="key" style="width: ${item.width};${
        item.marginLeft && "margin-left:" + item.marginLeft + "px"
      }">
      <div class="inner">
        <div class="${item.item === "Enter" && "lh40"}">${item.item}</div>
      </div>
    </div>`;
    }
  })
  .join("");

const elements5 = [{ item: "Shift", width: 125 }];

["Z", "X", "C", "V", "B", "N", "M"].forEach((char) => {
  elements5.push({
    item: char,
    width: 50,
  });
});

elements5.push({
  item: ["<", ","],
  width: 50,
});

elements5.push({
  item: [">", "."],
  width: 50,
});

elements5.push({
  item: ["?", "/"],
  width: 50,
});

elements5.push({ item: "Shift", width: 95 });

elements5.push({ item: "&#8593;", width: 50, align: "m" });

row5.innerHTML = elements5
  .map((item) => {
    if (Array.isArray(item.item)) {
      return `<div class="key" style="width: ${item.width}">
      <div class="inner flex-column space-between">
        <div class="f12 pl1">${item.item[0]}</div>
        <div class="f12 pl1">${item.item[1]}</div>
      </div>
    </div>`;
    } else {
      return `<div class="key" style="width: ${item.width}">
      <div class="inner ${item.align === "m" && "justify-center"}">
        <div class="${
          (item.item === "Shift" || item.align === "m") && "lh40"
        }">${item.item}</div>
      </div>
    </div>`;
    }
  })
  .join("");

const elements6 = [
  { name: "Ctrl", width: 66 },
  { name: "Win", width: 66 },
  { name: "Alt", width: 66 },
  { name: "", width: 312 },
  { name: "Alt", width: 66 },
  { name: "Fn", width: 50 },
  { name: "Ctrl", width: 66 },
  { name: "&#8592;", width: 50, align: "m" },
  { name: "&#8595;", width: 50, align: "m" },
  { name: "&#8594;", width: 50, align: "m" },
];

row6.innerHTML = elements6
  .map((item) => {
    if (item.top) {
      return `<div class="key">
      <div class="inner flex-column space-between">
        <div class="f12 pl1">${item.top}</div>
        <div class="f12 pl1">${item.bottom}</div>
      </div>
    </div>`;
    } else {
      return `<div class="key" data-key="${item.name}" style="width: ${item.width}">
      <div class="inner ${item.align === "m" && "justify-center"}">
        <div class="lh40">${item.name}</div>
      </div>
    </div>`;
    }
  })
  .join("");

const keyBtn = document.getElementsByClassName("key");

for (var i = 0; i < keyBtn.length; i++) {
  keyBtn[i].addEventListener("click", handleClick, false);
}

function handleClick(event) {
  const dataset = event.currentTarget.classList;
  let isActive = false;
  if (dataset) {
    isActive = !!Object.values(dataset).find(classname => classname.indexOf('key-active') >= 0)
  }
  if (isActive) {
    event.currentTarget.classList.remove("key-active");
  } else {
    for (var i = 0; i < keyBtn.length; i++) {
      keyBtn[i].classList.remove("key-active");
    }
    event.currentTarget.classList.add("key-active");
  }
}
