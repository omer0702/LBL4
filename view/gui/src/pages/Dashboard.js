import React from 'react';
import { useGlobal } from '../context/GlobalContext';
import StatCard from '../components/StatCard';
import ServiceCard from '../components/ServiceCard';
import { Activity, LayoutGrid, Zap, HeartPulse } from 'lucide-react';

const Dashboard = () => {
  const { services, globalStats, loading } = useGlobal();

  if (loading) return <div className="p-10">Loading Dashboard Data...</div>;

  return (
    <div className="p-8">
      {/* כרטיסי סיכום עליונים */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6 mb-10">
        <StatCard 
          icon={Zap} 
          label="Total Throughput" 
          value={`${globalStats.totalPps} PPS`} 
          colorClass="bg-blue-500" 
        />
        <StatCard 
          icon={LayoutGrid} 
          label="Active Services" 
          value={globalStats.activeServices} 
          colorClass="bg-purple-500" 
        />
        <StatCard 
          icon={Activity} 
          label="Connected Backends" 
          value={services.reduce((acc, s) => acc + s.active_backends, 0)} 
          colorClass="bg-lb-accent" 
        />
        <StatCard 
          icon={HeartPulse} 
          label="System Health" 
          value={globalStats.systemStatus} 
          colorClass={globalStats.systemStatus === 'Online' ? 'bg-green-500' : 'bg-red-500'} 
        />
      </div>

      {/* כותרת רשת השירותים */}
      <div className="flex items-center justify-between mb-6">
        <h2 className="text-xl font-bold flex items-center gap-2">
          Managed Services
          <span className="bg-slate-800 text-gray-400 text-xs px-2 py-1 rounded">{services.length}</span>
        </h2>
      </div>

      {/* רשת כרטיסי שירות */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
        {services.map(service => (
          <ServiceCard key={service.service_id} service={service} />
        ))}
      </div>
    </div>
  );
};

export default Dashboard;