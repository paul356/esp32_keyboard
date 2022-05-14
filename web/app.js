import './common.scss'
import './style.scss';


const buttonOk = document.getElementById("button_ok");
const buttonReset = document.getElementById("button_reset");
const row1 = document.getElementById("row_1");
const row2 = document.getElementById("row_2");
const row3 = document.getElementById("row_3");

buttonOk.onclick = function(event) {
  console.log(event);
}

buttonReset.onclick = function(event) {
  console.log(event);
}

const elements1 = ['Esc']
Array.from(Array(12).keys()).forEach(num => {
  elements1.push(`F${num + 1}`);
});
elements1.push('Delete');

row1.innerHTML = elements1.map(item => {
  return `<div class="key">
    <div class="inner">
      <div class="align-center pl1">${item}</div>
    </div>
  </div>`;
}).join('');

const elements2 = [
  {top: '~', bottom: '`'},
  {top: '!', bottom: '1'},
  {top: '@', bottom: '2'},
  {top: '#', bottom: '3'},
  {top: '$', bottom: '4'},
  {top: '%', bottom: '5'},
  {top: '^', bottom: '6'},
  {top: '&', bottom: '7'},
  {top: '*', bottom: '8'},
  {top: '(', bottom: '9'},
  {top: ')', bottom: '0'},
  {top: '_', bottom: '-'},
  {top: '+', bottom: '='},
  {name: "Backspace", width: 100},
]

row2.innerHTML = elements2.map(item => {
  if (item.top) {
    return `<div class="key">
      <div class="inner flex-column space-between">
        <div class="f12 pl1">${item.top}</div>
        <div class="f12 pl1">${item.bottom}</div>
      </div>
    </div>`;
  } else {
    return `<div class="key" style="width: ${item.width}">
      <div class="inner">
        <div>${item.name}</div>
      </div>
    </div>`;
  }
}).join('');