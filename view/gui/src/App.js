import React from "react";
import { BrowserRouter as Router, Routes, Route } from "react-router-dom";
import { GlobalProvider } from "./context/GlobalContext";
import Sidebar from "./components/Sidebar";
import Dashboard from './pages/Dashboard';
import ServiceDetails from './pages/ServiceDetails';

const PlaceHolder = ({ title }) => (
  <div className="p-8 text-2xl font-bold">
    {title} Page - Content Coming Soon!
  </div>
);


function App(){
  return(
    <GlobalProvider>
      <Router>
        <div className="flex h-screen overflow-hidden">
          <Sidebar />
          
          <main className="flex-1 overflow-y-auto bg-lb-dark">
            <header className="h-16 border-b border-slate-800 flex items-center px-8 justify-between">
              <h2 className="text-xl font-semibold text-lb-accent">System Dashboard</h2>
              <div className="flex items-center gap-4">
                <span className="flex items-center gap-2 text-sm text-green-400">
                  <span className="w-2 h-2 bg-green-400 rounded-full animate-pulse"></span>
                  System Online
                </span>
              </div>
            </header>

            <Routes>
              <Route path="/" element={<Dashboard />} />
              <Route path="/services" element={<ServiceDetails />} />
              <Route path="/services/:id" element={<ServiceDetails />} />
              <Route path="/traffic" element={<PlaceHolder title="Live Traffic" />} />
              <Route path="/logs" element={<PlaceHolder title="System Logs" />} />
              <Route path="/admin" element={<PlaceHolder title="Admin Control" />} />
            </Routes>
          </main>
        </div>
      </Router>
    </GlobalProvider>
  );



  // //   fetch("http://127.0.0.1:8000/api/hello")
  // //   .then((response)=>response.json())
  // //   .then((data)=>setBackendMessage(data.message))
  // //   .catch((error)=>console.error("error connecting to backend:",error));
  // // },[]);


  // useEffect(()=>{
  //   fetch("http://127.0.0.1:8000/api/test_data_from_db")
  //   .then((response)=>response.json())
  //   .then((data)=>setBackendMessage(data.services.join(", ")))
  //   .catch((error)=>console.error(error));
  // },[]);

  // return(
  //   <div style={{textAlign:"center", marginTop:"10px"}}>
  //     <h1>Frontend(react) connected to backend</h1>
  //     <h2>{backendMessage ? backendMessage : "connecting..."}</h2>
  //   </div>
  // );
}


export default App;
