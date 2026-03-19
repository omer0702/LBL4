/** @type {import('tailwindcss').Config} */
module.exports = {
  content: ["./src/**/*.{js,jsx,ts,tsx}"],
  theme: {
    extend: {
      colors: {
        'lb-dark': '#0f172a',
        'lb-sidebar': '#1e293b',
        'lb-accent': '#38bdf8',
      },
    },
  },
  plugins: [],
}

