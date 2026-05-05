import React from 'react';
import { Link, useLocation } from 'react-router-dom';
import { LayoutDashboard, Server, Activity, FileText, Settings, ShieldCheck } from 'lucide-react';

const SidebarItem = ({ icon: Icon, label, to , active}) => (
    <Link to={to} className={`flex items-center gap-3 px-6 py-4 transition-colors ${
        active ? 'bg-lb-accent text-lb-dark font-bold' : 'text-gray-400 hover:bg-slate-700 hover:text-white'
        }`}>
        <Icon size={20} />
        <span>{label}</span>
    </Link>
);

const Sidebar = () => {
    const location = useLocation();

    return (
        <div className="w-64 h-screen bg-lb-sidebar flex flex-col border-r border-slate-700">
            <div className="p-8 flex items-center gap-2 border-b border-slate-700">
                <ShieldCheck className="text-lb-accent" size={28} />
                <h1 className="text-lg font-bold tracking-tight">eBPF Load Distributer</h1>
            </div>

            <nav className="flex-1 mt-4">
                <SidebarItem icon={LayoutDashboard} label="Overview" to="/" active={location.pathname === '/'} />
                <SidebarItem icon={Server} label="Services" to="/services" active={location.pathname.startsWith('/services')} />
                <SidebarItem icon={Activity} label="Live Traffic" to="/live-traffic" active={location.pathname === '/live-traffic'} />
                <SidebarItem icon={FileText} label="System Logs" to="/logs" active={location.pathname === '/logs'} />
                <SidebarItem icon={Settings} label="Admin Control" to="/admin" active={location.pathname === '/admin'} />
            </nav>
            
            <div className="p-4 text-xs text-gray-500 border-t border-slate-700 text-center">
                v1.0.0 | L4 Load Balancer Dashboard
            </div>
        </div>
    );
};

export default Sidebar;