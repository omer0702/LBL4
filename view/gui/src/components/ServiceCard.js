import React from 'react';
import { useNavigate } from 'react-router-dom';
import { ChevronRight, Database } from 'lucide-react';

const ServiceCard = ({ service }) => {
  const navigate = useNavigate();
  const { active_backends, total_backends } = service;

  // לוגיקת סטטוס שירות
  let statusColor = "bg-red-500";
  let statusText = "Down";
  
  if (active_backends === total_backends && total_backends > 0) {
    statusColor = "bg-green-500";
    statusText = "Healthy";
  } else if (active_backends > 0) {
    statusColor = "bg-yellow-500";
    statusText = "Warning";
  }

  return (
    <div 
      onClick={() => navigate(`/services/${service.service_id}`)}
      className="bg-lb-sidebar rounded-xl border border-slate-700 p-5 hover:border-lb-accent transition-all cursor-pointer group"
    >
      <div className="flex justify-between items-start mb-4">
        <div>
          <h3 className="text-lg font-bold group-hover:text-lb-accent transition-colors">{service.name}</h3>
          <p className="text-gray-500 text-sm font-mono">{service.vip}</p>
        </div>
        <div className={`flex items-center gap-2 px-3 py-1 rounded-full text-xs font-bold ${statusColor} bg-opacity-10 ${statusColor.replace('bg-', 'text-')}`}>
          <span className={`w-2 h-2 rounded-full ${statusColor} animate-pulse`}></span>
          {statusText}
        </div>
      </div>

      <div className="grid grid-cols-2 gap-4 mt-6">
        <div className="flex flex-col">
          <span className="text-gray-500 text-xs">Backends</span>
          <span className="font-semibold">{active_backends} / {total_backends}</span>
        </div>
        <div className="flex flex-col">
          <span className="text-gray-500 text-xs">Throughput</span>
          <span className="font-semibold">{service.total_pps} PPS</span>
        </div>
      </div>

      <div className="mt-4 pt-4 border-t border-slate-800 flex justify-between items-center text-lb-accent text-sm font-medium">
        View Detailed Metrics
        <ChevronRight size={16} />
      </div>
    </div>
  );
};

export default ServiceCard;