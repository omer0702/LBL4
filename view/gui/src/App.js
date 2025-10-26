import React,{useEffect, useState} from "react";

function App(){
  const[backendMessage, setBackendMessage] = useState("");

  useEffect(()=>{
    fetch("http://127.0.0.1:8000/api/hello")
    .then((response)=>response.json())
    .then((data)=>setBackendMessage(data.message))
    .catch((error)=>console.error("error connecting to backend:",error));
  },[]);

  return(
    <div style={{textAlign:"center", marginTop:"50px"}}>
      <h1>Frontend(react) connected to backend</h1>
      <h2>{backendMessage ? backendMessage : "connecting..."}</h2>
    </div>
  );
}


export default App;
