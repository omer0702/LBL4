import React from 'react';

const StatCard = ({ icon: Icon, label, value, colorClass }) => (
  <div className="bg-lb-sidebar p-6 rounded-xl border border-slate-700 shadow-lg">
    <div className="flex items-center gap-4">
      <div className={`p-3 rounded-lg ${colorClass} bg-opacity-20`}>
        <Icon size={24} className={colorClass.replace('bg-', 'text-')} />
      </div>
      <div>
        <p className="text-gray-400 text-sm font-medium">{label}</p>
        <h3 className="text-2xl font-bold">{value}</h3>
      </div>
    </div>
  </div>
);

export default StatCard;