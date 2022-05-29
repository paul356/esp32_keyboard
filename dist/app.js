const buttonOk=document.getElementById("button_ok"),buttonReset=document.getElementById("button_reset"),row1=document.getElementById("row_1"),row2=document.getElementById("row_2"),row3=document.getElementById("row_3"),row4=document.getElementById("row_4"),row5=document.getElementById("row_5"),row6=document.getElementById("row_6"),elements1=(buttonOk.onclick=function(e){console.log(e)},buttonReset.onclick=function(e){console.log(e)},[{item:"Esc",width:50}]),elements2=(Array.from(Array(12).keys()).forEach(e=>{elements1.push({item:"F"+(e+1),width:50})}),elements1.push({item:"Delete",width:80,marginLeft:20}),elements1.push({item:"Home",width:50,marginLeft:40}),row1.innerHTML=elements1.map(e=>`<div class="key" style="width: ${e.width};${e.marginLeft?"margin-left:"+e.marginLeft+"px":""}">
    <div class="inner">
      <div class="align-center pl1 lh40" data-val="${e.item}">${e.item}</div>
    </div>
  </div>`).join(""),[{top:"~",bottom:"`"},{top:"!",bottom:"1"},{top:"@",bottom:"2"},{top:"#",bottom:"3"},{top:"$",bottom:"4"},{top:"%",bottom:"5"},{top:"^",bottom:"6"},{top:"&",bottom:"7"},{top:"*",bottom:"8"},{top:"(",bottom:"9"},{top:")",bottom:"0"},{top:"_",bottom:"-"},{top:"+",bottom:"="},{name:"Backspace",width:100}]),elements3=(elements2.push({name:"End",width:50,marginLeft:40}),row2.innerHTML=elements2.map(e=>e.top?`<div class="key">
      <div class="inner flex-column space-between">
        <div class="f12 pl1">${e.top}</div>
        <div class="f12 pl1">${e.bottom}</div>
      </div>
    </div>`:"End"===e.name?`<div class="key" style="width: ${e.width};${e.marginLeft&&"margin-left:"+e.marginLeft+"px"}">
      <div class="inner">
        <div class="align-center pl1 lh40">${e.name}</div>
      </div>
    </div>`:`<div class="key" style="width: ${e.width}">
      <div class="inner">
        <div class="${"Backspace"===e.name&&"lh40"}">${e.name}</div>
      </div>
    </div>`).join(""),[{item:"Tab",width:75}]),elements4=(["Q","W","E","R","T","Y","U","I","O","P"].forEach(e=>{elements3.push({item:e,width:50})}),elements3.push({item:["{","["],width:50}),elements3.push({item:["}","]"],width:50}),elements3.push({item:["|","\\"],width:75}),elements3.push({item:"Page Up",width:50,marginLeft:40}),row3.innerHTML=elements3.map(e=>Array.isArray(e.item)?`<div class="key" style="width: ${e.width}">
      <div class="inner flex-column space-between">
        <div class="f12 pl1">${e.item[0]}</div>
        <div class="f12 pl1">${e.item[1]}</div>
      </div>
    </div>`:`<div class="key" style="width: ${e.width};${e.marginLeft&&"margin-left:"+e.marginLeft+"px"}">
      <div class="inner">
        <div>${e.item}</div>
      </div>
    </div>`).join(""),[{item:"Caps Lock",width:100}]),elements5=(["A","S","D","F","G","H","J","K","L"].forEach(e=>{elements4.push({item:e,width:50})}),elements4.push({item:[":",";"],width:50}),elements4.push({item:['"',"'"],width:50}),elements4.push({item:"Enter",width:100}),elements4.push({item:"Page Down",width:50,marginLeft:40}),row4.innerHTML=elements4.map(e=>Array.isArray(e.item)?`<div class="key" style="width: ${e.width}">
      <div class="inner flex-column space-between">
        <div class="f12 pl1">${e.item[0]}</div>
        <div class="f12 pl1">${e.item[1]}</div>
      </div>
    </div>`:`<div class="key" style="width: ${e.width};${e.marginLeft&&"margin-left:"+e.marginLeft+"px"}">
      <div class="inner">
        <div class="${"Enter"===e.item&&"lh40"}">${e.item}</div>
      </div>
    </div>`).join(""),[{item:"Shift",width:125}]),elements6=(["Z","X","C","V","B","N","M"].forEach(e=>{elements5.push({item:e,width:50})}),elements5.push({item:["<",","],width:50}),elements5.push({item:[">","."],width:50}),elements5.push({item:["?","/"],width:50}),elements5.push({item:"Shift",width:95}),elements5.push({item:"&#8593;",width:50,align:"m"}),row5.innerHTML=elements5.map(e=>Array.isArray(e.item)?`<div class="key" style="width: ${e.width}">
      <div class="inner flex-column space-between">
        <div class="f12 pl1">${e.item[0]}</div>
        <div class="f12 pl1">${e.item[1]}</div>
      </div>
    </div>`:`<div class="key" style="width: ${e.width}">
      <div class="inner ${"m"===e.align&&"justify-center"}">
        <div class="${("Shift"===e.item||"m"===e.align)&&"lh40"}">${e.item}</div>
      </div>
    </div>`).join(""),[{name:"Ctrl",width:66},{name:"Win",width:66},{name:"Alt",width:66},{name:"",width:312},{name:"Alt",width:66},{name:"Fn",width:50},{name:"Ctrl",width:66},{name:"&#8592;",width:50,align:"m"},{name:"&#8595;",width:50,align:"m"},{name:"&#8594;",width:50,align:"m"}]),keyBtn=(row6.innerHTML=elements6.map(e=>e.top?`<div class="key">
      <div class="inner flex-column space-between">
        <div class="f12 pl1">${e.top}</div>
        <div class="f12 pl1">${e.bottom}</div>
      </div>
    </div>`:`<div class="key" data-key="${e.name}" style="width: ${e.width}">
      <div class="inner ${"m"===e.align&&"justify-center"}">
        <div class="lh40">${e.name}</div>
      </div>
    </div>`).join(""),document.getElementsByClassName("key"));for(var i=0;i<keyBtn.length;i++)keyBtn[i].addEventListener("click",handleClick,!1);function handleClick(e){var t=e.currentTarget.classList;let i=!1;if(i=t?!!Object.values(t).find(e=>0<=e.indexOf("key-active")):i)e.currentTarget.classList.remove("key-active");else{for(var n=0;n<keyBtn.length;n++)keyBtn[n].classList.remove("key-active");e.currentTarget.classList.add("key-active")}}
